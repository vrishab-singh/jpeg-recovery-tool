#include "mainwindow.h"
#include "ui_mainwindow.h"

// Original code
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    listWidget = new QListWidget(this);
    setCentralWidget(listWidget);

    process = new QProcess(this);
    connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(runCommand()));
    process->start("sudo", QStringList() << "fdisk" << "-l");

    QPushButton *selectOutputButton = new QPushButton("Select Output Path", this);
    connect(selectOutputButton, &QPushButton::clicked, this, &MainWindow::selectOutputPath);

    recoverButton = new QPushButton("Recover Data", this);
    connect(recoverButton, &QPushButton::clicked, this, &MainWindow::startRecovery);

    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setVisible(false); // Initially hide the cancel button
    connect(cancelButton, &QPushButton::clicked, this, &MainWindow::cancelRecovery);

    progressLabel = new QLabel("Recovery in progress...", this);
    progressLabel->setVisible(false);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // Set range to (0, 0) for an indeterminate progress bar
    progressBar->setVisible(false);

    // Create and set up the main layout
    QVBoxLayout *mainLayout = new QVBoxLayout();

    // Create the table widget
    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(4); // Updated to 4 columns
    QStringList headers = {"Device", "Size", "Type", "Disk Model"};
    tableWidget->setHorizontalHeaderLabels(headers);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows); // Ensure entire row is selected
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); // Make cells non-editable

    // Add widgets to the main layout
    mainLayout->addWidget(tableWidget);

    // Create button widget and layout
    QWidget *buttonWidget = new QWidget(this);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->addWidget(selectOutputButton);
    buttonLayout->addWidget(recoverButton);
    buttonLayout->addWidget(cancelButton);
    buttonWidget->setLayout(buttonLayout);

    // Add button widget to the main layout
    mainLayout->addWidget(buttonWidget);
    mainLayout->addWidget(progressLabel);
    mainLayout->addWidget(progressBar);

    // Set the main layout to a central widget
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // Initialize timer
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateProgressBar);

    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::readProcessOutput);

}


MainWindow::~MainWindow()
{
    delete process;
    delete tableWidget;
}

void MainWindow::runCommand()
{
    QString output = process->readAllStandardOutput();
    QStringList lines = output.split("\n", Qt::SkipEmptyParts);
    tableWidget->setRowCount(0); // Clear existing rows
    QString currentDiskModel;

    for (QString line : lines) { // Make a copy of the line to allow modifications
        // Extract disk model
        if (line.startsWith("Disk model:")) {
            currentDiskModel = line.section("Disk model:", 1).trimmed();
            continue;
        }

        if (line.startsWith("Disk /dev/")) {
            currentDiskModel.clear();
        }

        if (line.startsWith("/dev/")) {
            line.remove('*'); // Modify the copy of the line
            qDebug() << "--> " << line;
            QStringList fields;
            for (const QString& field : line.split(" ", Qt::SkipEmptyParts)) { // Use Qt::SkipEmptyParts to skip empty fields
                fields.append(field);
            }
            if (fields.size() < 6) {
                continue;
            }
            int row = tableWidget->rowCount();
            tableWidget->insertRow(row);

            // Insert Device
            tableWidget->setItem(row, 0, new QTableWidgetItem(fields.at(0))); // Device

            // Insert Size
            QString size = fields.at(4); // Corrected index for Size
            tableWidget->setItem(row, 1, new QTableWidgetItem(size)); // Size

            // Insert Type
            QString type;
            for (int i = 5; i < fields.size(); ++i) {
                // Check if the field is numeric
                bool isNumeric = false;
                for (QChar ch : fields.at(i)) {
                    if (ch.isDigit()) {
                        isNumeric = true;
                        break;
                    }
                }
                // If the field is not numeric, add it to the type
                if (!isNumeric) {
                    if (!type.isEmpty()) type += " "; // Add space between fields
                    type += fields.at(i);
                }
            }
            tableWidget->setItem(row, 2, new QTableWidgetItem(type));

            // Insert Disk Model
            tableWidget->setItem(row, 3, new QTableWidgetItem(currentDiskModel)); // Disk Model

            // Alternate row colors
            for (int col = 0; col < tableWidget->columnCount(); ++col) {
                if (row % 2 == 0) {
                    tableWidget->item(row, col)->setBackground(Qt::white);
                } else {
                    tableWidget->item(row, col)->setBackground(QColor("#DEECF9"));
                }
            }
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        QString selectedText;
        QList<QTableWidgetItem *> selectedItems = tableWidget->selectedItems();
        for (int i = 0; i < selectedItems.size(); ++i) {
            selectedText += selectedItems.at(i)->text();
            if (i < selectedItems.size() - 1)
                selectedText += "\n";
        }
        if (!selectedText.isEmpty())
            QGuiApplication::clipboard()->setText(selectedText);
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::selectOutputPath()
{
    QString outputPath = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
    if (!outputPath.isEmpty()) {
        this->outputPath = outputPath;
    }
}

void MainWindow::startRecovery()
{
    QList<QTableWidgetItem *> selectedItems = tableWidget->selectedItems(); // Corrected to QTableWidget
    if (selectedItems.isEmpty()) {
        QMessageBox::critical(this, "Error", "Please select a volume/partition/drive for recovery.");
        return;
    }

    if (outputPath.isEmpty()) {
        QMessageBox::critical(this, "Error", "Please select an output directory path.");
        return;
    }

    recoverButton->setEnabled(false);
    cancelButton->setVisible(true); // Show the cancel button
    progressLabel->setVisible(true);
    progressBar->setVisible(true);

    // Start the timer to update progress bar
    timer->start(50);

    QString selectedDisk = selectedItems.first()->text().split(" ").first(); // Get only the disk name
    qDebug() << "selectedDisk: " << selectedDisk;

    // Add double quotes around outputPath
    QString quotedOutputPath = "\"" + outputPath + "\"";
    qDebug().noquote() << "outputPath: " << quotedOutputPath;

    QString command = QString("sudo recoverjpeg -o %1 %2").arg(quotedOutputPath, selectedDisk);
    qDebug().noquote() << "command: " << command;

    // Redirect standard output to Qt Creator output
    process->setProcessChannelMode(QProcess::MergedChannels);

    // Start the process
    process->start(command);

    // Connect the readyReadStandardOutput signal to read the output of the process
    connect(process, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray output = process->readAllStandardOutput();
        qDebug().noquote() << output;
    });

    // Connect the finished signal to handle the completion of the process
    connect(process, QOverload<int>::of(&QProcess::finished), this, &MainWindow::recoveryFinished);

}

void MainWindow::readProcessOutput()
{
//    QByteArray data = process->readAllStandardOutput();
//    qDebug() << "Terminal Output:" << data;
}

void MainWindow::updateProgressBar()
{
    // Implement the functionality to update the progress bar here
}

void MainWindow::stopProgressBar()
{
    progressBar->setVisible(false);
    timer->stop();
}

void MainWindow::recoveryFinished(int exitCode)
{
    recoverButton->setEnabled(true);
    cancelButton->setVisible(false); // Hide the cancel button
    progressLabel->setVisible(false);
    progressBar->setVisible(false);

    // Stop the timer when recovery finished
    timer->stop();

    if (exitCode == 0) {
        qDebug() << "Recovery done!";
        QMessageBox::information(this, "Recovery Completed", QString("Recovery completed successfully.\nOutput Path: %1").arg(outputPath));
    } else {
        qDebug() << "Recovery failed!";
        QMessageBox::critical(this, "Recovery Failed", "Recovery process failed.");
    }
}

void MainWindow::cancelRecovery()
{
    process->kill(); // Kill the recovery process
    recoverButton->setEnabled(true);
    cancelButton->setVisible(false); // Hide the cancel button
    progressLabel->setVisible(false);
    progressBar->setVisible(false);
    timer->stop(); // Stop the timer
}

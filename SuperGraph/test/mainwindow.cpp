#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Shapeholder.h"
#include "Matcher.h"
#include "Supergraph.h"
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Change window to preferred size
    this->ui->centralWidget->setMinimumSize(QSize(1280, 720));
    this->resize(QSize(this->sizeHint()) );
    //this->resize(QSize(200,200));
    this->ui->centralWidget->setMinimumSize(QSize(0,0));

    // Center to screen
    QDesktopWidget* m = QApplication::desktop();
    QRect desk_rect = m->screenGeometry(m->screenNumber(QCursor::pos()));
    int desk_x = desk_rect.width();
    int desk_y = desk_rect.height();
    int x = this->width();
    int y = this->height();
    this->move(desk_x / 2 - x / 2 + desk_rect.left(), desk_y / 2 - y / 2 + desk_rect.top());

    Prepare();
}

void MainWindow::Prepare(){
    // Everything is ready, load shapes now:
    QString datasetFolder = "dataset";
    DatasetMap datasetMap = getDataset(datasetFolder);

    // Ask user for path of dataset
    while( datasetMap.isEmpty() ){
        //打开一个对话框选择目录
        datasetFolder = QFileDialog::getExistingDirectory();
        datasetMap = getDataset(datasetFolder);
        if(datasetMap.isEmpty())
            QMessageBox::critical(this, "Cannot find data", "Cannot find data in this folder. Please restart and choose a correct one.");
    }

    ShapeHolder * holder = new ShapeHolder();
    holder->loadDataset(datasetMap);
    Matcher * matcher = new Matcher(holder->inputGraphs);
    matcher->doMatch();
    SuperGraph * supergraph = new SuperGraph(matcher->gcorr);
    supergraph->generateSuperGraphs();
    supergraph->saveMatchedGraph(QString("target.xml"));

    return;
}

DatasetMap MainWindow::getDataset(QString datasetPath)
{
    DatasetMap dataset;

    QDir datasetDir(datasetPath);   //列出所有文件：列出符合要求的目录|列出符号链接（不支持符号连接的操作系统会忽略）|不要列出特殊入口"." and ".."
    QStringList subdirs = datasetDir.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);

    foreach(QString subdir, subdirs)
    {
        // Special folders
        if(subdir == "corr") continue;

        QDir d(datasetPath + "/" + subdir);

        // Check if no graph is in this folder
        if( d.entryList(QStringList() << "*.xml", QDir::Files).isEmpty() ) continue;
        //subdir是一个shape的名字，后面的是PropertyMap的成员QVarient
        dataset[subdir]["Name"] = subdir;
        dataset[subdir]["graphFile"] = d.absolutePath() + "/" + d.entryList(QStringList() << "*.xml", QDir::Files).join("");
        dataset[subdir]["thumbFile"] = d.absolutePath() + "/" + d.entryList(QStringList() << "*.png", QDir::Files).join("");
        dataset[subdir]["objFile"] = d.absolutePath() + "/" + d.entryList(QStringList() << "*.obj", QDir::Files).join("");
    }

    return dataset;
}

MainWindow::~MainWindow()
{
    delete ui;
}

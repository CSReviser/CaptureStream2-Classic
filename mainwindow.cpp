/*
	Copyright (C) 2009-2013 jakago

	This file is part of CaptureStream, the flv downloader for NHK radio
	language courses.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "downloadthread.h"
#include "customizedialog.h"
#include "scrambledialog.h"
#include "utility.h"
#include "qt4qt5.h"

#ifdef QT5
#include <QXmlQuery>
#include <QDesktopWidget>
#include <QRegExp>
#endif
#ifdef QT6
#include <QRegularExpression>
#endif
#include <QMessageBox>
#include <QByteArray>
#include <QStringList>
#include <QProcess>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QSettings>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QtNetwork>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>
#include <QFont>
#include <QDesktopServices>

#define SETTING_GROUP "MainWindow"
#define SETTING_GEOMETRY "geometry"
#define SETTING_WINDOWSTATE "windowState"
#define SETTING_MAINWINDOW_POSITION "Mainwindow_Position"
#define SETTING_SAVE_FOLDER "save_folder"
#define SETTING_SCRAMBLE "scramble"
#define SETTING_SCRAMBLE_URL1 "scramble_url1"
#define SETTING_SCRAMBLE_URL2 "scramble_url2"
#define SETTING_FILE_NAME1 "FILE_NAME1"
#define SETTING_FILE_NAME2 "FILE_NAME2"
#define SETTING_TITLE1 "FILE_TITLE1"
#define SETTING_TITLE2 "FILE_TITLE2"
#define FILE_NAME1 "%k_%Y_%M_%D"
#define FILE_NAME2 "%k_%Y_%M_%D"
#define FILE_TITLE1 "%f"
#define FILE_TITLE2 "%k_%Y_%M_%D"
#define SCRAMBLE_URL1 "http://www47.atwiki.jp/jakago/pub/scramble.xml"
#define SCRAMBLE_URL2 "http://cdn47.atwikiimg.com/jakago/pub/scramble.xml"
#define X11_WINDOW_VERTICAL_INCREMENT 5

#ifdef QT4_QT5_WIN
#define STYLE_SHEET "stylesheet-win.qss"
#else
#ifdef QT4_QT5_MAC
#define STYLE_SHEET "stylesheet-mac.qss"
#else
#define STYLE_SHEET "stylesheet-ubu.qss"
#endif
#endif

namespace {
	bool outputDirSpecified = false;
	QString version() {
		QString result;
		// 日本語ロケールではQDate::fromStringで曜日なしは動作しないのでQRegExpを使う
		// __DATE__の形式： "Jul  8 2011"
#ifdef QT5
		static QRegExp regexp( "([a-zA-Z]{3})\\s+(\\d{1,2})\\s+(\\d{4})" );
		static QStringList months = QStringList()
				<< "Jan" << "Feb" << "Mar" << "Apr" << "May" << "Jun"
				<< "Jul" << "Aug" << "Sep" << "Oct" << "Nov" << "Dec";
		if ( regexp.indexIn( __DATE__ ) != -1 ) {
//			int month = months.indexOf( regexp.cap( 1 ) ) + 1;
//			int day = regexp.cap( 2 ).toInt();
//			result = QString( " (%1/%2/%3)" ).arg( regexp.cap( 3 ) )
//					.arg( month, 2, 10, QLatin1Char( '0' ) ).arg( day, 2, 10, QLatin1Char( '0' ) );
			result = QString( "  (2024/06/05) -classic-" ); 
		}
#endif
#ifdef QT6
			result = QString( "  (2024/06/05) -classic-" ); 
#endif
		return result;
	}
}

QString MainWindow::outputDir;
QString MainWindow::ini_file_path;
QString MainWindow::scramble;
QString MainWindow::scrambleUrl1;
QString MainWindow::scrambleUrl2;
QString MainWindow::customized_title1;
QString MainWindow::customized_title2;
QString MainWindow::customized_file_name1;
QString MainWindow::customized_file_name2;
QString MainWindow::prefix = "http://cgi2.nhk.or.jp/gogaku/st/xml/";
QString MainWindow::suffix = "listdataflv.xml";
QString MainWindow::json_prefix = "https://www.nhk.or.jp/radioondemand/json/";
QString MainWindow::no_write_ini;
bool MainWindow::id_flag = false;

MainWindow::MainWindow( QWidget *parent )
		: QMainWindow( parent ), ui( new Ui::MainWindowClass ), downloadThread( NULL ) {
#ifdef QT4_QT5_MAC
	ini_file_path = Utility::ConfigLocationPath();
#endif
#if !defined( QT4_QT5_MAC )
	ini_file_path = Utility::applicationBundlePath();
#endif	
	ui->setupUi( this );
	settings( ReadMode );
	this->setWindowTitle( this->windowTitle() + version() );
	no_write_ini = "yes";
	
#ifdef QT4_QT5_MAC		// Macのウィンドウにはメニューが出ないので縦方向に縮める
//	setMaximumHeight( maximumHeight() - menuBar()->height() );
//	setMinimumHeight( maximumHeight() - menuBar()->height() );
	menuBar()->setNativeMenuBar(false);		// 他のOSと同様にメニューバーを表示　2023/04/04
	setMaximumHeight( maximumHeight() );		// ダウンロードボタンが表示されない問題対策　2022/04/16
	setMinimumHeight( maximumHeight() );		// ダウンロードボタンが表示されない問題対策　2022/04/16
//	QRect rect = geometry();
//	rect.setHeight( rect.height() - menuBar()->height() );
//	rect.setHeight( rect.height() );		// ダウンロードボタンが表示されない問題対策　2022/04/16
//	rect.moveTop( rect.top() + menuBar()->height() );	// 4.6.3だとこれがないとウィンドウタイトルがメニューバーに隠れる
//	setGeometry( rect );
#endif
#ifdef Q_OS_LINUX		// Linuxでは高さが足りなくなるので縦方向に伸ばしておく
	menuBar()->setNativeMenuBar(false);					// メニューバーが表示されなくなったに対応
	setMaximumHeight( maximumHeight() + X11_WINDOW_VERTICAL_INCREMENT );
	setMinimumHeight( maximumHeight() + X11_WINDOW_VERTICAL_INCREMENT );
	QRect rect = geometry();
	rect.setHeight( rect.height() + X11_WINDOW_VERTICAL_INCREMENT );
	setGeometry( rect );
#endif

#if !defined( QT4_QT5_MAC ) && !defined( QT4_QT5_WIN )
	QPoint bottomLeft = geometry().bottomLeft();
	bottomLeft += QPoint( 0, menuBar()->height() + statusBar()->height() + 3 );
	messagewindow.move( bottomLeft );
#endif

	// 「カスタマイズ」メニューの構築
	customizeMenu = menuBar()->addMenu( QString::fromUtf8( "カスタマイズ" ) );

	QAction* action = new QAction( QString::fromUtf8( "保存フォルダ設定..." ), this );
	connect( action, SIGNAL( triggered() ), this, SLOT( customizeSaveFolder() ) );
	customizeMenu->addAction( action );
	action = new QAction( QString::fromUtf8( "保存フォルダ開く..." ), this );
	connect( action, SIGNAL( triggered() ), this, SLOT( customizeFolderOpen() ) );
	customizeMenu->addAction( action );
	customizeMenu->addSeparator();

	action = new QAction( QString::fromUtf8( "ファイル名設定..." ), this );
	connect( action, SIGNAL( triggered() ), this, SLOT( customizeFileName() ) );
	customizeMenu->addAction( action );

	action = new QAction( QString::fromUtf8( "タイトルタグ設定..." ), this );
	connect( action, SIGNAL( triggered() ), this, SLOT( customizeTitle() ) );
	customizeMenu->addAction( action );
	customizeMenu->addSeparator();

	customizeMenu->addSeparator();
	action = new QAction( QString::fromUtf8( "設定削除（終了）..." ), this );
	connect( action, SIGNAL( triggered() ), this, SLOT( closeEvent2() ) );
	customizeMenu->addAction( action );

#ifdef QT4_QT5_MAC
	QFont font( "Times New Roman", 10 );
	qApp->setFont(font);
#endif
#ifdef QT4_QT5_WIN
	QFont font( "Meiryo", 8 );
	qApp->setFont(font);
#endif
#ifdef Q_OS_LINUX
	QFont font( "Times New Roman", 9 );
	qApp->setFont(font);
#endif

}

MainWindow::~MainWindow() {
	if ( downloadThread ) {
		downloadThread->terminate();
		delete downloadThread;
	}
	if ( !Utility::nogui() && no_write_ini == "yes" )
		settings( WriteMode );
	delete ui;
}

void MainWindow::closeEvent( QCloseEvent *event ) {
	Q_UNUSED( event )
	if ( downloadThread ) {
		messagewindow.appendParagraph( QString::fromUtf8( "レコーディングをキャンセル中..." ) );
		download();
	}
	messagewindow.close();
	QCoreApplication::exit();
}

void MainWindow::settings( enum ReadWriteMode mode ) {
	typedef struct CheckBox {
		QCheckBox* checkBox;
		QString key;
		QVariant defaultValue;
	} CheckBox;
#define DefaultTitle "%k_%Y_%M_%D"
//#define DefaultTitle1 "%f"
//#define DefaultTitle2 "%k_%Y_%M_%D"
#define DefaultFileName "%k_%Y_%M_%D.m4a"
//#define DefaultFileName1 "%k_%Y_%M_%D"
//#define DefaultFileName2 "%k_%Y_%M_%D"
//#define DefaultFileName3 "%h"
//#define DefaultFileName4 "%f"
	CheckBox checkBoxes[] = {
		{ ui->checkBox_basic0, "basic0", false },
		{ ui->checkBox_basic1, "basic1", false },
		{ ui->checkBox_basic2, "basic2", false },
		{ ui->checkBox_basic3, "basic3", false },
		{ ui->checkBox_timetrial, "timetrial", false },
		{ ui->checkBox_kaiwa, "kaiwa", false },
		{ ui->checkBox_business1, "business1", false },
		{ ui->checkBox_enjoy, "enjoy", false },
		{ ui->checkBox_vrradio, "vrradio", false },
		{ ui->checkBox_gendai, "gendai", false },
		{ ui->checkBox_chinese, "chinese", false },
		{ ui->checkBox_hangeul, "hangeul", false },
		{ ui->checkBox_french, "french", false },
		{ ui->checkBox_italian, "italian", false },
		{ ui->checkBox_german, "german", false },
		{ ui->checkBox_spanish, "spanish", false },
		{ ui->checkBox_russian, "russian", false },
		{ ui->checkBox_portuguese, "portuguese", false },
		{ ui->checkBox_arabic, "arabic", false },
		{ ui->checkBox_japanese, "japanese", false },
		{ ui->checkBox_shower, "shower", false },
		{ ui->checkBox_skip, "skip", true },
		{ ui->checkBox_keep_on_error, "keep_on_error", false },
		{ ui->checkBox_this_week, "this_week", true },
		{ ui->checkBox_next_week, "next_week", false },
		{ ui->checkBox_next_week2, "past_week", false },
		{ NULL, NULL, false }
	};

	typedef struct ComboBox {
		QComboBox* comboBox;
		QString key;
		QVariant defaultValue;
	} ComboBox;
	ComboBox comboBoxes[] = {
//		{ ui->comboBox_enews, "e-news-index", ENewsSaveBoth },
//		{ ui->comboBox_shower, "shower_index", ENewsSaveBoth },
		{ NULL, NULL, false }
	};
	ComboBox textComboBoxes[] = {
		{ ui->comboBox_extension, "audio_extension", "m4a" },	// 拡張子のデフォルトを「mp3」から「m4a」に変更。
		{ NULL, NULL, false }
	};
	
	
	QSettings settings( ini_file_path + INI_FILE, QSettings::IniFormat );
	
	settings.beginGroup( SETTING_GROUP );

	if ( mode == ReadMode ) {	// 設定読み込み
		QVariant saved;
		
#if !defined( QT4_QT5_MAC )
//#if defined( QT4_QT5_MAC ) || defined( QT4_QT5_WIN )	// X11では正しく憶えられないので位置をリストアしない(2022/11/01:Linux向けに変更）
		saved = settings.value( SETTING_GEOMETRY );
#ifdef QT5
		if ( saved.type() == QVariant::Invalid )
#endif
#ifdef QT6
		if ( saved.toString() == "" )
#endif
			move( 70, 22 );
		else {
			// ウィンドウサイズはバージョン毎に変わる可能性があるのでウィンドウ位置だけリストアする
			QSize windowSize = size();
			restoreGeometry( saved.toByteArray() );
			resize( windowSize );
		}
//#endif                                              　//(2022/11/01:Linux向けに変更） 
#endif
#ifdef QT4_QT5_MAC
		saved = settings.value( SETTING_MAINWINDOW_POSITION );
#ifdef QT5
		if ( saved.type() == QVariant::Invalid ){
#endif
#ifdef QT6
		if ( saved.toString() == "" ){
#endif
			move( 70, 22 );
			QRect rect = geometry();
			rect.setHeight( rect.height() );		// ダウンロードボタンが表示されない問題対策　2022/04/16
			rect.moveTop( rect.top() );	// 4.6.3だとこれがないとウィンドウタイトルがメニューバーに隠れる
			setGeometry( rect );
		} else {
			QSize windowSize = size();
			move( saved.toPoint() );
			resize( windowSize );
		}
		saved = settings.value( SETTING_WINDOWSTATE );
		if ( !(saved.type() == QVariant::Invalid) )
			restoreState( saved.toByteArray() );
#endif

		saved = settings.value( SETTING_SAVE_FOLDER );
#if !defined( QT4_QT5_MAC )
#ifdef QT5
		outputDir = saved.type() == QVariant::Invalid ? Utility::applicationBundlePath() : saved.toString();
#endif
#ifdef QT6
		outputDir = saved.toString() == "" ? Utility::applicationBundlePath() : saved.toString();
#endif
#endif
#ifdef QT4_QT5_MAC
#ifdef QT5
		if ( saved.type() == QVariant::Invalid ) {
#endif
#ifdef QT6
		if ( saved.toString() == "" ) {
#endif
			outputDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
			MainWindow::customizeSaveFolder();
		} else
			outputDir = saved.toString();
#endif

		for ( int i = 0; checkBoxes[i].checkBox != NULL; i++ ) {
			checkBoxes[i].checkBox->setChecked( settings.value( checkBoxes[i].key, checkBoxes[i].defaultValue ).toBool() );
		}
		for ( int i = 0; comboBoxes[i].comboBox != NULL; i++ )
			comboBoxes[i].comboBox->setCurrentIndex( settings.value( comboBoxes[i].key, comboBoxes[i].defaultValue ).toInt() );
		for ( int i = 0; textComboBoxes[i].comboBox != NULL; i++ ) {
			QString extension = settings.value( textComboBoxes[i].key, textComboBoxes[i].defaultValue ).toString();
			textComboBoxes[i].comboBox->setCurrentIndex( textComboBoxes[i].comboBox->findText( extension ) );
		}
	} else {	// 設定書き出し
#if !defined( QT4_QT5_MAC )
		settings.setValue( SETTING_GEOMETRY, saveGeometry() );
#endif
#ifdef QT4_QT5_MAC
		settings.setValue( SETTING_WINDOWSTATE, saveState());
		settings.setValue( SETTING_MAINWINDOW_POSITION, pos() );
#endif
		if ( outputDirSpecified )
			settings.setValue( SETTING_SAVE_FOLDER, outputDir );
		
		for ( int i = 0; checkBoxes[i].checkBox != NULL; i++ ) {
			settings.setValue( checkBoxes[i].key, checkBoxes[i].checkBox->isChecked() );
		}
		for ( int i = 0; comboBoxes[i].comboBox != NULL; i++ )
			settings.setValue( comboBoxes[i].key, comboBoxes[i].comboBox->currentIndex() );
		for ( int i = 0; textComboBoxes[i].comboBox != NULL; i++ )
			settings.setValue( textComboBoxes[i].key, textComboBoxes[i].comboBox->currentText() );
	}

	settings.endGroup();
}

void MainWindow::customizeTitle() {
	CustomizeDialog dialog( Ui::TitleMode );
	dialog.exec();
}

void MainWindow::customizeFileName() {
	CustomizeDialog dialog( Ui::FileNameMode );
	dialog.exec();
}

void MainWindow::customizeFolderOpen() {
	QDesktopServices::openUrl(QUrl("file:///" + outputDir, QUrl::TolerantMode));
}

void MainWindow::customizeSaveFolder() {
	QString dir = QFileDialog::getExistingDirectory( 0, QString::fromUtf8( "書き込み可能な保存フォルダを指定してください" ),
									   outputDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
	if ( dir.length() ) {
		outputDir = dir + QDir::separator();
		outputDirSpecified = true;
	}
}

#if 0
	if ( mode == ReadMode ) {	// 設定読み込み
		QVariant saved;
		
//#if !defined( QT4_QT5_MAC )
//#if defined( QT4_QT5_MAC ) || defined( QT4_QT5_WIN )	// X11では正しく憶えられないので位置をリストアしない(2022/11/01:Linux向けに変更）
		saved = settings.value( SETTING_GEOMETRY );
#ifdef QT5
		if ( saved.type() == QVariant::Invalid )
#endif
#ifdef QT6
		if ( saved.toString() == "" )
#endif
			move( 70, 22 );
		else {
			// ウィンドウサイズはバージョン毎に変わる可能性があるのでウィンドウ位置だけリストアする
			QSize windowSize = size();
			restoreGeometry( saved.toByteArray() );
			resize( windowSize );
		}
//#endif                                              　//(2022/11/01:Linux向けに変更） 
//#endif
#if 0
//#ifdef QT4_QT5_MAC
		saved = settings.value( SETTING_MAINWINDOW_POSITION );
		if ( saved.type() == QVariant::Invalid )
			move( 70, 22 );
		else {
			QSize windowSize = size();
			move( saved.toPoint() );
			resize( windowSize );
		}
		saved = settings.value( SETTING_WINDOWSTATE );
		if ( !(saved.type() == QVariant::Invalid) )
			restoreState( saved.toByteArray() );
#endif

		saved = settings.value( SETTING_SAVE_FOLDER );
#if !defined( QT4_QT5_MAC )
#ifdef QT5
		outputDir = saved.type() == QVariant::Invalid ? Utility::applicationBundlePath() : saved.toString();
#endif
#ifdef QT6
		outputDir = saved.toString() == "" ? Utility::applicationBundlePath() : saved.toString();
#endif
#endif
#ifdef QT4_QT5_MAC
#ifdef QT5
		if ( saved.type() == QVariant::Invalid ) {
#endif
#ifdef QT6
		if ( saved.toString() == "" ) {
#endif
			outputDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
			MainWindow::customizeSaveFolder();
		} else
			outputDir = saved.toString();
#endif
	} else {	// 設定書き出し
#if !defined( QT4_QT5_MAC )
		settings.setValue( SETTING_GEOMETRY, saveGeometry() );
#endif
#ifdef QT4_QT5_MAC
		settings.setValue( SETTING_WINDOWSTATE, saveState());
		settings.setValue( SETTING_MAINWINDOW_POSITION, pos() );
#endif
		if ( outputDirSpecified )
			settings.setValue( SETTING_SAVE_FOLDER, outputDir );
	}

	settings.endGroup();
}

void MainWindow::customizeTitle() {
	CustomizeDialog dialog( Ui::TitleMode );
	dialog.exec();
}

void MainWindow::customizeFileName() {
	CustomizeDialog dialog( Ui::FileNameMode );
	dialog.exec();
}

void MainWindow::customizeSaveFolder() {
	QString dir = QFileDialog::getExistingDirectory( 0, QString::fromUtf8( "書き込み可能な保存フォルダを指定してください" ),
									   outputDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
	if ( dir.length() ) {
		outputDir = dir + QDir::separator();
		outputDirSpecified = true;
	}
}

#if 0
void MainWindow::customizeScramble() {
	QString optional_temp[] = { optional1, optional2, optional3, optional4, optional5, optional6, optional7, optional8, "NULL" };
	ScrambleDialog dialog( optional1, optional2, optional3, optional4, optional5, optional6, optional7, optional8 );
    if (dialog.exec() ) {
    	QString pattern( "[0-9]{4}" );
    	pattern = QRegularExpression::anchoredPattern(pattern);
	for ( int i = 0; optional_temp[i] != "NULL"; i++ ) 
	    	if ( QRegularExpression(pattern).match( optional_temp[i] ).hasMatch() ) optional_temp[i] += "_01";

	QString optional[] = { dialog.scramble1(), dialog.scramble2(), dialog.scramble3(), dialog.scramble4(), dialog.scramble5(), dialog.scramble6(), dialog.scramble7(), dialog.scramble8(), "NULL" };	
	QString title[8];
	for ( int i = 0; optional[i] != "NULL"; i++ ) {
		if ( QRegularExpression(pattern).match( optional[i] ).hasMatch() ) optional[i] += "_01" ;
		title[i] = Utility::getProgram_name( optional[i] );
		if ( title[i]  == "" ) { optional[i] = optional_temp[i]; title[i] = Utility::getProgram_name( optional[i] ); }
	}
	optional1 = optional[0]; optional2 = optional[1];
	optional3 = optional[2]; optional4 = optional[3];
	optional5 = optional[4]; optional6 = optional[5];
	optional7 = optional[6]; optional8 = optional[7];
	program_title1 = title[0]; program_title2 = title[1];
	program_title3 = title[2]; program_title4 = title[3];
	program_title5 = title[4]; program_title6 = title[5];
	program_title7 = title[6]; program_title8 = title[7];

	QString program_title[] = { program_title1, program_title2, program_title3, program_title4, program_title5, program_title6, program_title7, program_title8, "NULL" };
	QAbstractButton* checkboxx[] = { ui->toolButton_optional1, ui->toolButton_optional2,
					 ui->toolButton_optional3, ui->toolButton_optional4,
					 ui->toolButton_optional5, ui->toolButton_optional6,
					 ui->toolButton_optional7, ui->toolButton_optional8,
					 NULL
		 	};
	bool flag = false;
	for ( int i = 0; program_title[i] != "NULL"; i++ ) {
		if ( optional[i] == optional_temp[i] && checkboxx[i]->isChecked() ) flag = true; else flag = false;
				checkboxx[i]->setChecked(false);
				checkboxx[i]->setText( QString( program_title[i] ) );
				if ( flag ) checkboxx[i]->setChecked( true );
	}
	optional1 = optional[0]; optional2 = optional[1]; optional3 = optional[2]; optional4 = optional[3];
	optional5 = optional[4]; optional6 = optional[5]; optional7 = optional[6]; optional8 = optional[7];
	ScrambleDialog dialog( optional1, optional2, optional3, optional4, optional5, optional6, optional7, optional8 );
    }
}
#endif
#endif
void MainWindow::download() {	//「レコーディング」または「キャンセル」ボタンが押されると呼び出される
	if ( !downloadThread ) {	//レコーディング実行
		if ( messagewindow.text().length() > 0 )
			messagewindow.appendParagraph( "\n----------------------------------------" );
		ui->downloadButton->setEnabled( false );
		downloadThread = new DownloadThread( ui );
		connect( downloadThread, SIGNAL( finished() ), this, SLOT( finished() ) );
		connect( downloadThread, SIGNAL( critical( QString ) ), &messagewindow, SLOT( appendParagraph( QString ) ), Qt::BlockingQueuedConnection );
		connect( downloadThread, SIGNAL( information( QString ) ), &messagewindow, SLOT( appendParagraph( QString ) ), Qt::BlockingQueuedConnection );
		connect( downloadThread, SIGNAL( current( QString ) ), &messagewindow, SLOT( appendParagraph( QString ) ) );
		connect( downloadThread, SIGNAL( messageWithoutBreak( QString ) ), &messagewindow, SLOT( append( QString ) ) );
		downloadThread->start();
		ui->downloadButton->setText( QString::fromUtf8( "キャンセル" ) );
		ui->downloadButton->setEnabled( true );
	} else {	//キャンセル
		downloadThread->disconnect();	//wait中にSIGNALが発生するとデッドロックするためすべてdisconnect
		finished();
	}
}

void MainWindow::toggled( bool checked ) {
//	QObject* sender = this->sender();
//	if ( sender ) {
//		QToolButton* button = (QToolButton*)sender;
//		QString text = button->text();
		if ( checked ) return;
//			text.insert( 0, QString::fromUtf8( "✓ " ) );
//		else
//			text.remove( 0, 2 );
//		button->setText( text );
//	}
}

void MainWindow::finished() {
	if ( downloadThread ) {
		ui->downloadButton->setEnabled( false );
		if ( downloadThread->isRunning() ) {	//キャンセルでMainWindow::downloadから呼ばれた場合
			downloadThread->cancel();
			downloadThread->wait();
			messagewindow.appendParagraph( QString::fromUtf8( "レコーディングをキャンセルしました。" ) );
		}
		delete downloadThread;
		downloadThread = NULL;
		ui->downloadButton->setText( QString::fromUtf8( "レコーディング" ) );
		ui->downloadButton->setEnabled( true );
	}
	//ui->label->setText( "" );
	if ( Utility::nogui() )
		QCoreApplication::exit();
}

void MainWindow::closeEvent2( ) {
	int res = QMessageBox::question(this, tr("設定削除"), tr("削除しますか？"));
	if (res == QMessageBox::Yes) {
	no_write_ini = "no";
	
	QFile::remove( ini_file_path + INI_FILE );
	
	if ( downloadThread ) {
		messagewindow.appendParagraph( QString::fromUtf8( "レコーディングをキャンセル中..." ) );
		download();
	}
	messagewindow.close();
	QCoreApplication::exit();
	}
}


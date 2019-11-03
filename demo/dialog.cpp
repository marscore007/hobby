#include "dialog.h"
#include "ui_dialog.h"
#include "QtWebSockets/qwebsocketserver.h"
#include <QMenu>
#include "QtWebSockets/qwebsocket.h"
#include "magicwidgetitem.h"
#include "fillcommand.h"

const int MSG_TYPE_LOGIN = 0;
const int  MSG_TYPE_INIT = 1;
const int MSG_TYPE_SYNC = 2;
const int MSG_TYPE_RESULT = 3;
const int MSG_TYPE_COMMAND = 4;
const int MSG_TYPE_Auth = 5;

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
	QStringList header;
	header << QString::fromStdWString(L"状态") << QString::fromStdWString(L"用户名") << QString::fromStdWString(L"昵称");
	ui->tableWidget->setColumnCount(4);
	ui->tableWidget->setHorizontalHeaderLabels(header);
	ui->tableWidget->setColumnWidth(0, 90);  //0 设置列宽
	ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pSendCommand = new QAction(QString::fromStdWString(L"发送命令"));
	connect(m_pSendCommand, SIGNAL(triggered()), this, SLOT(OnSendCommand()));
	connect(ui->tableWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(show_menu(QPoint)));

	m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Echo Server"),
		QWebSocketServer::NonSecureMode, this);
	if (m_pWebSocketServer->listen(QHostAddress::Any, 1234)) {
		connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
			this, &Dialog::onNewConnection);
		connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &Dialog::closed);
	}
}

Dialog::~Dialog()
{
	m_pWebSocketServer->close();
    delete ui;
}

void Dialog::HandleLogin(QWebSocket *pClient, const QJsonObject& json_root)
{
	CMagicWidgetItem *item = nullptr;
	for (int i = 0; i < ui->tableWidget->rowCount(); i++)
	{
		CMagicWidgetItem* pItem = (CMagicWidgetItem*)ui->tableWidget->item(i, 0);
		if (pItem->m_pSocket == pClient)
		{
			item = pItem;
			break;
		}
	}

	int iErrorCode = json_root["base"].toObject()["errorcode"].toInt();
	QString strTip;
	if (iErrorCode == -1)
	{
		strTip = QString::fromStdWString(L"空闲");
		//发送命令授权,只需要发送一次就可以
		QJsonObject base;
		base.insert("id", m_iTaskID++);
		base.insert("type", MSG_TYPE_Auth);
		base.insert("errorcode", 0);

		QJsonObject command;
		command.insert("base", base);
		command.insert("account", "xxxx");
		command.insert("token", "xxxxx");
		
		QJsonDocument document;
		document.setObject(command);
		QByteArray byteArray = document.toJson(QJsonDocument::Compact);

		pClient->sendTextMessage(QString(byteArray));
	}
	else if (iErrorCode == 0)
	{
		strTip = QString::fromStdWString(L"在线");
	}
	else if (iErrorCode == -301)
	{
		strTip = QString::fromStdWString(L"重定向");
		//第一次登录都会有这个错误，只要重新登陆一次就可以
	}
	else
	{
		strTip = QString::fromStdWString(L"异常");
	}
	if (item != nullptr)
	{
		item->setText(strTip);
	}
	else
	{
		item = new CMagicWidgetItem(strTip, pClient);
		int iRow = ui->tableWidget->rowCount();
		ui->tableWidget->insertRow(iRow);
		ui->tableWidget->setItem(iRow, 0, item);
	}
	
}

void Dialog::HandleInit(QWebSocket *pClient, const QJsonObject& json_root)
{

}

void Dialog::HandleSync(QWebSocket *pClient, const QJsonObject& json_root)
{

}

void Dialog::HandleResult(QWebSocket *pClient, const QJsonObject& json_root)
{

}

void Dialog::socketDisconnected()
{
	QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
	if (pClient) {
		auto it = m_clients.find(pClient);
		if (it != m_clients.end())
		{
			for (int i = 0; i < ui->tableWidget->rowCount();i++)
			{
				CMagicWidgetItem* pItem = (CMagicWidgetItem*)ui->tableWidget->item(i, 0);
				if (pItem->m_pSocket == pClient)
				{
					ui->tableWidget->removeRow(i);
					break;
				}
			}
			m_clients.erase(it);
		}
		pClient->deleteLater();
	}
}

void Dialog::show_menu(const QPoint pos)
{
	QMenu *menu = new QMenu(ui->tableWidget);
	menu->addAction(m_pSendCommand);
	menu->move(cursor().pos());
	menu->show();
}

void Dialog::OnSendCommand()
{
	int current_row = ui->tableWidget->currentRow();
	if (current_row == -1)
	{
		return;
	}
	CFillCommand dlg(this);
	int iCode = dlg.exec();
	if (iCode ==  QDialog::Accepted)
	{
		CMagicWidgetItem* pItem = (CMagicWidgetItem*)ui->tableWidget->item(current_row, 0);
		if (nullptr == pItem)
		{
			return;
		}

		//通过websocket 发送消息
		QJsonObject base;
		base.insert("id", m_iTaskID++);
		base.insert("type", MSG_TYPE_COMMAND);
		base.insert("errorcode", 0);

		QJsonObject command;
		command.insert("base", base);
		command.insert("command", dlg.m_strCommand);

		QJsonDocument document;
		document.setObject(command);
		QByteArray byteArray = document.toJson(QJsonDocument::Compact);

		pItem->m_pSocket->sendTextMessage(QString(byteArray));
	}
}

//客户端连接成功之后，保存客户端的连接
void Dialog::onNewConnection()
{
	QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

	connect(pSocket, &QWebSocket::textMessageReceived, this, &Dialog::processTextMessage);
	connect(pSocket, &QWebSocket::binaryMessageReceived, this, &Dialog::processBinaryMessage);
	connect(pSocket, &QWebSocket::disconnected, this, &Dialog::socketDisconnected);
	m_clients[pSocket] = ClientInfo();
}

//接收到客户端发来的消息，是一个json 处理消息
void Dialog::processTextMessage(QString message)
{
	ui->textBrowser->insertPlainText(message);
	ui->textBrowser->insertPlainText("\n");
	QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
	QJsonParseError jsonError;
	QJsonDocument doucment = QJsonDocument::fromJson(message.toUtf8(), &jsonError);
	if (!doucment.isObject())
	{
		return;
	}
	QJsonObject root = doucment.object();
	QJsonObject base = root["base"].toObject();
	switch (base["type"].toInt())
	{
	case MSG_TYPE_LOGIN:
	{
		HandleLogin(pClient, root);
	}
	break;
	case MSG_TYPE_INIT:
	{
		HandleInit(pClient, root);
	}
	break;
	case MSG_TYPE_SYNC:
	{
		HandleSync(pClient, root);
	}
	break;
	case MSG_TYPE_RESULT:
	{
		HandleResult(pClient, root);
	}
	break;
	default:
		break;
	}

}

void Dialog::processBinaryMessage(QByteArray message)
{
	QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
	if (pClient) {
		pClient->sendBinaryMessage(message);
	}
}
void Dialog::on_pushButton_clicked(bool checked)
{
    ui->textBrowser->clear();
}

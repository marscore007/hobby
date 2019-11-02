#include "dialog.h"
#include "ui_dialog.h"
#include "QtWebSockets/qwebsocketserver.h"
#include <QMenu>
#include "QtWebSockets/qwebsocket.h"
#include "magicwidgetitem.h"
#include "fillcommand.h"
Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
	QStringList header;
	header << QString::fromStdWString(L"标识符") << QString::fromStdWString(L"状态") << QString::fromStdWString(L"用户名") << QString::fromStdWString(L"昵称");
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

void Dialog::HandleWakeup(QWebSocket *pClient, const QJsonObject& json_root)
{
	/**
	 * {
	 "base": {
	 "id": 0,
	 "type": 0,
	 "errorcode": 0
	 },
	 "state": 1,
	 "identify": "abc",
	 "account": "abc",
	 "username": "wxid_",
	 "nickname": "nickname"
	 }
	 */
	CMagicWidgetItem *item = new CMagicWidgetItem(json_root["identify"].toString(),pClient);
	int iRow = ui->tableWidget->rowCount();
	ui->tableWidget->insertRow(iRow);
	ui->tableWidget->setItem(iRow, 0, item);
	if (json_root["state"].toInt() == 0)
	{
		ui->tableWidget->setItem(iRow, 1, new QTableWidgetItem(json_root["在线"].toString()));
	}
	else
	{
		ui->tableWidget->setItem(iRow, 1, new QTableWidgetItem(json_root["离线"].toString()));
	}
	ui->tableWidget->setItem(iRow, 2, new QTableWidgetItem(json_root["account"].toString()));
	ui->tableWidget->setItem(iRow, 3, new QTableWidgetItem(json_root["nickname"].toString()));
}

void Dialog::HandleLogin(QWebSocket *pClient, const QJsonObject& json_root)
{

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
		QJsonObject base;
		base.insert("id", m_iTaskID++);
		base.insert("type", 5);
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

void Dialog::onNewConnection()
{
	QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

	connect(pSocket, &QWebSocket::textMessageReceived, this, &Dialog::processTextMessage);
	connect(pSocket, &QWebSocket::binaryMessageReceived, this, &Dialog::processBinaryMessage);
	connect(pSocket, &QWebSocket::disconnected, this, &Dialog::socketDisconnected);
	m_clients[pSocket] = ClientInfo();
}

void Dialog::processTextMessage(QString message)
{
	ui->textBrowser->insertPlainText(message);
	QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
	QJsonParseError jsonError;
	QJsonDocument doucment = QJsonDocument::fromJson(message.toLocal8Bit(), &jsonError);
	if (!doucment.isObject())
	{
		return;
	}
	QJsonObject root = doucment.object();
	QJsonObject base = root["base"].toObject();
	switch (base["type"].toInt())
	{
	case 0:
	{
		HandleWakeup(pClient, root);
	}
	break;
	case 1:
	{
		HandleLogin(pClient, root);
	}
	break;
	case 2:
	{
		HandleInit(pClient, root);
	}
	break;
	case 3:
	{
		HandleSync(pClient, root);
	}
	break;
	case 4:
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
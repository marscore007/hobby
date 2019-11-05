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
        command.insert("account", "xxx");
        command.insert("token", "xxx");
		
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
	m_pGlobalSocket = nullptr;
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
		SendCommand(pItem->m_pSocket,dlg.m_strCommand);
	}
}

//客户端连接成功之后，保存客户端的连接
void Dialog::onNewConnection()
{
	QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
	m_pGlobalSocket = pSocket;
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

void Dialog::on_login_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 0);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", "17714315310");
	jsonObject.insert("strPasswd", "dms5761");
	jsonObject.insert("Env", "{\n"
		"\t\"DeviceBrand\": \"google\",\n" 
		"\t\"DeviceGUID\": \"A19ed38c0c4e2b59\",\n" 
		"\t\"DeviceInfo\": \"<deviceinfo><MANUFACTURER name=\\\\\\\"LGE\\\\\\\"><MODEL name=\\\\\\\"Nexus 5\\\\\\\"><VERSION_RELEASE name=\\\\\\\"9.0\\\\\\\"><VERSION_INCREMENTAL name=\\\\\\\"eng.android.20190330.165024\\\\\\\"><DISPLAY name=\\\\\\\"LMY48B\\\\\\\"></DISPLAY></VERSION_INCREMENTAL></VERSION_RELEASE></MODEL></MANUFACTURER></deviceinfo>\",\n" 
		"\t\"DeviceName\": \"LGE-Nexus 5\",\n" 
		"\t\"DeviceType\": \"android-28\",\n" 
		"\t\"IMEI\": \"359251150300373\",\n" 
		"\t\"Language\": \"zh_CN\",\n" 
		"\t\"RealCountry\": \"cn\",\n" 
		"\t\"SoftType\": \"<softtype><lctmoc>0</lctmoc><level>0</level><k1>ARMv7 Processor rev 0 (v7l) </k1><k2>M8974A-2.0.50.1.16</k2><k3>9.0</k3><k4>359251150300373</k4><k5>460040893100044</k5><k6>898602</k6><k7>916321c8de41f9f5</k7><k8>34c7c082d0241c90</k8><k9>Nexus 5</k9><k10>4</k10><k11>Qualcomm MSM 8974 HAMMERHEAD (Flattened Device Tree)</k11><k12>000b</k12><k13>0000000000000000</k13><k14>1c:76:7c:06:98:8a</k14><k15>BC:F5:AC:6D:D8:C3</k15><k16>swp half thumb fastmult vfp edsp neon vfpv3 tls vfpv4 idiva idivt</k16><k18>18c867f0717aa67b2ab7347505ba07ed</k18><k21>xxx</k21><k22>CMCC</k22><k24>68:db:54:5c:03:f6</k24><k26>0</k26><k30>&quot;xxx&quot;</k30><k33>com.tencent.mm</k33><k34>Android/aosp_hammerhead/hammerhead:9.0/LMY48M/android03301653:userdebug/test-keys</k34><k35>hammerhead</k35><k36>HHZ11k</k36><k37>google</k37><k38>hammerhead</k38><k39>hammerhead</k39><k40>aosp_hammerhead</k40><k41>1</k41><k42>LGE</k42><k43>null</k43><k44>0</k44><k45></k45><k46></k46><k47>wifi</k47><k48>359251150300373</k48><k49>/data/data/com.tencent.mm/</k49><k52>0</k52><k53>0</k53><k57>1381</k57><k58></k58><k59>3</k59><k60></k60><k61>true</k61></softtype>\",\n" 
		"\t\"TimeZone\": \"8.00\"\n" 
		"}");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::SendCommand(QWebSocket *pClient, const QString &strCommand)
{
	if (nullptr == pClient)
	{
		return;
	}
    QJsonObject base;
    base.insert("id", m_iTaskID++);
    base.insert("type", MSG_TYPE_COMMAND);
    base.insert("errorcode", 0);

    QJsonObject command;
    command.insert("base", base);
	command.insert("command", strCommand);

    QJsonDocument document;
    document.setObject(command);
    QByteArray byteArray = document.toJson(QJsonDocument::Compact);
    pClient->sendTextMessage(QString(byteArray));
}

void Dialog::SendCommand(QWebSocket *pClient, const QJsonObject &command)
{
	QJsonDocument document;
	document.setObject(command);
	QByteArray byteArray = document.toJson(QJsonDocument::Compact);
	SendCommand(pClient, QString(byteArray));
}

void Dialog::on_autologin_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 70);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", "17714315310");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_snsprivacy_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 73);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iModFlag", ui->value1->text().toInt());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_searchcontact_clicked(bool checked)
{
    QJsonObject jsonObject;
    QJsonObject jsonBase;
    jsonBase.insert("msgType", 3);
    jsonObject.insert("base", jsonBase);
    jsonObject.insert("iSearchScene", 0);
    jsonObject.insert("strUserName", ui->value1->text());
    SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_verifycontact_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 4);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strV2", ui->value2->text());
	jsonObject.insert("strIntroduce", "xiexie");
	jsonObject.insert("strChatroom", "");
	jsonObject.insert("iOpCode", 2);
	jsonObject.insert("iScene", 3);
	jsonObject.insert("iFriendFlag", 0);// 1：不看我的朋友圈
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendtext_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 8);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strContent", ui->value2->text());
	jsonObject.insert("strAt", "xxx");
	jsonObject.insert("iType", 1);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendpic_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 9);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strPath", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendvoice_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 10);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strPath", ui->value2->text());
	jsonObject.insert("iTime", 10000);
	jsonObject.insert("iVoiceFormat", 0); //0->Arm 无amr音频头 2->MP3 3->WAVE 4->SILK
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getmfriend_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 11);
	jsonObject.insert("base", jsonBase);

	QJsonArray phones;
	if (!ui->value1->text().isEmpty())
	{
		phones.append(ui->value1->text());
	}
	if (!ui->value2->text().isEmpty())
	{
		phones.append(ui->value2->text());
	}
	jsonObject.insert("Phones", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_uploadmfriend_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 12);
	jsonObject.insert("base", jsonBase);

	QJsonArray phones;
	if (!ui->value1->text().isEmpty())
	{
		phones.append(ui->value1->text());
	}
	if (!ui->value2->text().isEmpty())
	{
		phones.append(ui->value2->text());
	}
	jsonObject.insert("Phones", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getcontact_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 5);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strChatroom", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getqrcode_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 16);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getimage_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 17);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strFromUserName", "xxx@chatroom");
	jsonObject.insert("strToUserName", "xxx");
	jsonObject.insert("iFileTotalLen", 56847);
	jsonObject.insert("SvrID", "xxx");
	jsonObject.insert("iCompress", 0);
	jsonObject.insert("strPath", "/sdcard/abc.jpg");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getvoice_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonObject.insert("msgType", 18);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iMsgId", "xxx");
	jsonObject.insert("strClientMsgId", "xxx@xxx");
	jsonObject.insert("strPath", "/sdcard/abc.amr");
	jsonObject.insert("iTotalLen", 5688);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendapp_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 20);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strToUserName", ui->value1->text());
	jsonObject.insert("strXMLContent", "<appmsg appid=\"xxx\" sdkver=\"0\">\n\t\t<title>水枪玩具夏天儿童沙滩电动强力喷水枪玩具背包男女孩宝宝漂流戏水洗澡玩具 电动水枪</title>\n\t\t<des>我在京东发现了一个不错的商品，赶快来看看吧。</des>\n\t\t<action />\n\t\t<type>5</type>\n\t\t<showtype>0</showtype>\n\t\t<soundtype>0</soundtype>\n\t\t<mediatagname />\n\t\t<messageext />\n\t\t<messageaction />\n\t\t<content />\n\t\t<contentattr>0</contentattr>\n\t\t<url>https://item.m.jd.com/product/43176489417.html?wxa_abtest=o&amp;utm_source=iosapp&amp;utm_medium=appshare&amp;utm_campaign=t_335139774&amp;utm_term=Wxfriends&amp;ad_od=share</url>\n\t\t<lowurl />\n\t\t<dataurl />\n\t\t<lowdataurl />\n\t\t<appattach>\n\t\t\t<totallen>0</totallen>\n\t\t\t<attachid />\n\t\t\t<emoticonmd5 />\n\t\t\t<fileext />\n\t\t\t<cdnthumburl>3056020100044f304d0201000204239dd1b402033d14bb020490105e6402045d3c07070428777875706c6f61645f777869645f79793965746f7736617068323232365f313536343231353034370204010800030201000400</cdnthumburl>\n\t\t\t<cdnthumbmd5>9ffec2d5417696a21eed157517cb3991</cdnthumbmd5>\n\t\t\t<cdnthumblength>5536</cdnthumblength>\n\t\t\t<cdnthumbwidth>120</cdnthumbwidth>\n\t\t\t<cdnthumbheight>120</cdnthumbheight>\n\t\t\t<cdnthumbaeskey>40441c1f42fd44e2c96db4bc27e48524</cdnthumbaeskey>\n\t\t\t<aeskey>40441c1f42fd44e2c96db4bc27e48524</aeskey>\n\t\t\t<encryver>0</encryver>\n\t\t\t<filekey>xxx_yy9etow6aph2226_1564215047</filekey>\n\t\t</appattach>\n\t\t<extinfo />\n\t\t<sourceusername />\n\t\t<sourcedisplayname />\n\t\t<thumburl />\n\t\t<md5 />\n\t\t<statextstr>GhQKEnd4ZTc1YTJlNjg4NzczMTVmYg==</statextstr>\n\t</appmsg>");
	jsonObject.insert("iType", 5);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_uploadhead_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 21);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strPath", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_snsupload_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 22);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iType", ui->value1->text().toInt());//0图片 1视频
	jsonObject.insert("strPath", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendsns_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 23);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strContent", "xxxxxx");
	jsonObject.insert("iType", 0); //0图片 1视频  2 改变朋友圈背景

	QJsonArray params  ;
	QJsonObject p1  ;
	p1.insert("iVideoLen", 0);
	p1.insert("iImgSize", 0);
	p1.insert("iImgWidth", 224);
	p1.insert("iImgHeight", 391);
	p1.insert("strImgUrl", "http://mmsns.qpic.xxx5gagRibLgTVr/0");
	p1.insert("strThumbnailURL", "http://mmsns.qpic.cn/mxxxppSuDI7ib5gagRibLgTVr/150");
	params.append(p1);

	jsonObject.insert("params", params);

	//whitelist
	QJsonArray whiteList;
	jsonObject.insert("whiteList", whiteList);
	//blacklist

	QJsonArray blackList;
	jsonObject.insert("BlackList", blackList);

	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getsnstimeline_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 24);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strMinID", "0");
	jsonObject.insert("strMaxID", "0");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_snscomment_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 25);
  jsonObject.insert("base", jsonBase);
  jsonObject.insert("strComment", "xxxx");
  jsonObject.insert("strToUserName", ui->value1->text());
  jsonObject.insert("iType", 1);////1 点赞 2 评论
  jsonObject.insert("strSNSID", ui->value2->text());
  SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_usersnstimeline_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 77);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strMinID", "0");
	jsonObject.insert("strMaxID", "0");
	jsonObject.insert("strUserName", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getprofile_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 26);
	jsonObject.insert("base", jsonBase);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_createchatroom_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 27);
	jsonObject.insert("base", jsonBase);

	QJsonArray phones ;
	phones.append(ui->value1->text());
	phones.append(ui->value2->text());
	jsonObject.insert("MemberLists", phones);
	jsonObject.insert("strChatroomName", "来啊来啊");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_addchatmember_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 28);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", ui->value1->text());
	QJsonArray phones ;
	phones.append(ui->value2->text());
	jsonObject.insert("MemberLists", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_invitechatmember_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 29);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID",ui->value1->text());
	QJsonArray phones ;
	phones.append(ui->value2->text());
	jsonObject.insert("MemberLists", phones);
	jsonObject.insert("strIntroduce", "test");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_delchatmember_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 30);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", ui->value1->text());
	QJsonArray phones ;
	phones.append(ui->value2->text());
	jsonObject.insert("MemberLists", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_approveaddmember_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 32);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", "");
	jsonObject.insert("strTicket", "");
	jsonObject.insert("strInvite", "");

	QJsonArray phones;
	phones.append("xxxxxx");
	jsonObject.insert("MemberLists", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_transferchatroom_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 33);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", ui->value1->text());
	jsonObject.insert("strUserName", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_chatroomanoment_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 34);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strChatroomID", ui->value1->text());
	jsonObject.insert("strContent", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_setweichatnumber_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 36);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strWeichatNumber", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_delcontact_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 38);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modprop_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 39);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strChatroom", "");
	jsonObject.insert("iProp", ui->value2->text().toInt());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modmarkphone_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 40);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strUserName", ui->value1->text());

	QJsonArray phones;
	phones.append(ui->value2->text());
	jsonObject.insert("Phones", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modroomname_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 42);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strChatroomID", ui->value1->text());
	jsonObject.insert("strChatroomName", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_validroom_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 43);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", ui->value1->text());
	jsonObject.insert("iType", ui->value2->text().toInt());//0 取消 2 开启
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_leaveroom_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 44);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strChatroomID", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_haspasswd_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 48);
	jsonObject.insert("base", jsonBase); SendCommand(m_pGlobalSocket, jsonObject);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_checkpasswdd_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 49);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("Passwd", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_setpasswd_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 50);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("Passwd", ui->value1->text());
	jsonObject.insert("Ticket", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modnickname_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 53);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("NickName", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modsignature_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 54);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("Signature", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getlabel_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 60);
	jsonObject.insert("base", jsonBase);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_updatelabel_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 63);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strLabelName", ui->value1->text());
	jsonObject.insert("iLabelID", ui->value2->text().toInt());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_dellabel_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 62);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strLabelLists", ui->value1->text());

	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_addlabel_clicked(bool checked)
{

	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 61);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strLabelName", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modcontactlabel_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 64);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strLabelLists", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendcdnvideo_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 65);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("strAESKey", "xxx");
	jsonObject.insert("strCDNVideoUrl", "306b02010004643062020xxxx833cb702045d3be293043d61757076696465xxxx23435646435625f313536xx00");
	jsonObject.insert("iThumbTotalLen", 31814);
	jsonObject.insert("iVideoLen", 214586);
	jsonObject.insert("iPlayLen", 1);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_modroommemnickname_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 69);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strChatroomID", ui->value1->text());
	jsonObject.insert("strUserName", ui->value2->text());
	jsonObject.insert("strNickName", "xxxx");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_masssendtext_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 71);
	jsonObject.insert("base", jsonBase);

	jsonObject.insert("strUserLists", ui->value1->text());
	jsonObject.insert("iUserCount", ui->value2->text().toInt());
	jsonObject.insert("iType", 1);
	jsonObject.insert("strContent", "xxxxx");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_snsprivate_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 73);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iModFlag", ui->value1->text().toInt());//2689 半年 3201 三天  2177 全部
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_ForbidenSnsEd_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 75);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("iFlag", ui->value2->text().toInt()); //不让他看我的 1 开启 2 关闭
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_ForbidenSns_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 76);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strUserName", ui->value1->text());
	jsonObject.insert("iFlag", ui->value2->text().toInt()); //不看他的 1 开启 2 关闭
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_timelineopt_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 78);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iSNSID", ui->value1->text());
	jsonObject.insert("iType", ui->value2->text().toInt()); //1 删除 2 设为私密  3  设为公开
	SendCommand(m_pGlobalSocket, jsonObject);	
}

void Dialog::on_scanqrcodelogin_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 80);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strReqUrl", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_bindmobile_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 83);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strPhone", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_verifymobile_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 84);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strPhone", ui->value1->text());
	jsonObject.insert("strTicket", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getsafedevice_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 86);
	jsonObject.insert("base", jsonBase);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_delsafedevice_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 87);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strDeviceID", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getvideo_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 88);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iMsgId", ui->value1->text());
	jsonObject.insert("strPath", "/sdcard/abc.mp4");
	jsonObject.insert("iTotalLen", ui->value2->text().toInt());

	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_batchgethead_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 89);
	jsonObject.insert("base", jsonBase);
	
	QJsonArray phones;
	phones.append(ui->value1->text());
	phones.append(ui->value2->text());
	jsonObject.insert("UserNameList", phones);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_revokemsg_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 90);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("iClientMsgId", 1234);
	jsonObject.insert("iCreateTime", 11111);
	jsonObject.insert("strFromUserName", "111");
	jsonObject.insert("strToUser", "111");
	jsonObject.insert("NewMsgId", "xxxxx");
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getemoillist_clicked(bool checked)
{
	QJsonObject jsonObject ;
	QJsonObject jsonBase ;
	jsonBase.insert("msgType", 94);
	jsonObject.insert("base", jsonBase);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getemodetail_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 93);
	jsonObject.insert("base", jsonBase); 
	jsonObject.insert("strProductID", ui->value1->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_sendemoji_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 92);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("strToUserName", ui->value1->text());
	jsonObject.insert("iTotalLen", 31089);//自己根据url 下载回来计算大小  CDNUrl 字段
	jsonObject.insert("strMD5", ui->value2->text());
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_addmetype_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 95);
	jsonObject.insert("base", jsonBase);
	
	QJsonArray array;
	QJsonObject type1;
	type1.insert("function", 1);
	type1.insert("value", 1);
	array.append(type1);

	jsonObject.insert("CmdList", array);
	SendCommand(m_pGlobalSocket, jsonObject);
}

void Dialog::on_getpoi_clicked(bool checked)
{
	QJsonObject jsonObject;
	QJsonObject jsonBase;
	jsonBase.insert("msgType", 96);
	jsonObject.insert("base", jsonBase);
	jsonObject.insert("dLatitude", 141.1111111);
	jsonObject.insert("dLongitude", 32.22222222);
	SendCommand(m_pGlobalSocket, jsonObject);
}

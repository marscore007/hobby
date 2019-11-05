#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <map>
QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

namespace Ui {
class Dialog;
}

struct ClientInfo{

};

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();

Q_SIGNALS:
	void closed();

	private Q_SLOTS:
	void onNewConnection();
	void processTextMessage(QString message);
	void processBinaryMessage(QByteArray message);
	void socketDisconnected();
	
private slots:
	void show_menu(const QPoint pos);
	void OnSendCommand();
	void on_pushButton_clicked(bool checked);
	void on_login_clicked(bool checked);
	
    void on_autologin_clicked(bool checked);

    void on_snsprivacy_clicked(bool checked);

    void on_searchcontact_clicked(bool checked);

    void on_verifycontact_clicked(bool checked);

    void on_sendtext_clicked(bool checked);

    void on_sendpic_clicked(bool checked);

    void on_sendvoice_clicked(bool checked);

    void on_getmfriend_clicked(bool checked);

    void on_uploadmfriend_clicked(bool checked);

    void on_getcontact_clicked(bool checked);

    void on_getqrcode_clicked(bool checked);

    void on_getimage_clicked(bool checked);

    void on_getvoice_clicked(bool checked);

    void on_sendapp_clicked(bool checked);

    void on_uploadhead_clicked(bool checked);

    void on_snsupload_clicked(bool checked);

    void on_sendsns_clicked(bool checked);

    void on_getsnstimeline_clicked(bool checked);

    void on_snscomment_clicked(bool checked);

    void on_usersnstimeline_clicked(bool checked);

    void on_getprofile_clicked(bool checked);

    void on_createchatroom_clicked(bool checked);

    void on_addchatmember_clicked(bool checked);

    void on_invitechatmember_clicked(bool checked);

    void on_delchatmember_clicked(bool checked);

    void on_approveaddmember_clicked(bool checked);

    void on_transferchatroom_clicked(bool checked);

    void on_chatroomanoment_clicked(bool checked);

    void on_setweichatnumber_clicked(bool checked);

    void on_delcontact_clicked(bool checked);

    void on_modprop_clicked(bool checked);

    void on_modmarkphone_clicked(bool checked);

    void on_modroomname_clicked(bool checked);

    void on_validroom_clicked(bool checked);

    void on_leaveroom_clicked(bool checked);

    void on_haspasswd_clicked(bool checked);

    void on_checkpasswdd_clicked(bool checked);

    void on_setpasswd_clicked(bool checked);

    void on_modnickname_clicked(bool checked);

    void on_modsignature_clicked(bool checked);

    void on_getlabel_clicked(bool checked);

    void on_updatelabel_clicked(bool checked);

    void on_dellabel_clicked(bool checked);

    void on_addlabel_clicked(bool checked);

    void on_modcontactlabel_clicked(bool checked);

    void on_sendcdnvideo_clicked(bool checked);

    void on_modroommemnickname_clicked(bool checked);

    void on_masssendtext_clicked(bool checked);

    void on_snsprivate_clicked(bool checked);

    void on_ForbidenSnsEd_clicked(bool checked);

    void on_ForbidenSns_clicked(bool checked);

    void on_timelineopt_clicked(bool checked);

    void on_scanqrcodelogin_clicked(bool checked);

    void on_bindmobile_clicked(bool checked);

    void on_verifymobile_clicked(bool checked);

    void on_getsafedevice_clicked(bool checked);

    void on_delsafedevice_clicked(bool checked);

    void on_getvideo_clicked(bool checked);

    void on_batchgethead_clicked(bool checked);

    void on_revokemsg_clicked(bool checked);

    void on_getemoillist_clicked(bool checked);

    void on_getemodetail_clicked(bool checked);

    void on_sendemoji_clicked(bool checked);

    void on_addmetype_clicked(bool checked);

    void on_getpoi_clicked(bool checked);

private:
	void HandleLogin(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleInit(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleSync(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleResult(QWebSocket *pClient, const QJsonObject& json_root);
	void SendCommand(QWebSocket *pClient, const QString &command);
	void SendCommand(QWebSocket *pClient, const QJsonObject &command);

private:
	QWebSocket *m_pGlobalSocket = nullptr;
	QWebSocketServer *m_pWebSocketServer;
	std::map<QWebSocket *, ClientInfo> m_clients;
    Ui::Dialog *ui;
	QAction* m_pSendCommand;
	int m_iTaskID = 1;
};

#endif // DIALOG_H

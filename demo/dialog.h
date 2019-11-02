#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonDocument>
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

private:
	void HandleWakeup(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleLogin(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleInit(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleSync(QWebSocket *pClient, const QJsonObject& json_root);
	void HandleResult(QWebSocket *pClient, const QJsonObject& json_root);

private:
	QWebSocketServer *m_pWebSocketServer;
	std::map<QWebSocket *, ClientInfo> m_clients;
    Ui::Dialog *ui;
	QAction* m_pSendCommand;
	int m_iTaskID = 1;
};

#endif // DIALOG_H

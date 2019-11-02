#ifndef MAGICWIDGETITEM_H
#define MAGICWIDGETITEM_H
#include <QWebSocket>
#include <QTableWidgetItem>
class CMagicWidgetItem:public QTableWidgetItem
{
public:
    CMagicWidgetItem(const QString& text,QWebSocket* pSocket);

    QWebSocket* m_pSocket;
};

#endif // MAGICWIDGETITEM_H

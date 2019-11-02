#include "magicwidgetitem.h"

CMagicWidgetItem::CMagicWidgetItem(const QString& text,QWebSocket* pSocket):
    QTableWidgetItem(text),
	m_pSocket(pSocket)
{

}

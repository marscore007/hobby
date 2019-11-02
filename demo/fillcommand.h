#ifndef FILLCOMMAND_H
#define FILLCOMMAND_H

#include <QDialog>

namespace Ui {
class CFillCommand;
}

class CFillCommand : public QDialog
{
    Q_OBJECT

public:
    explicit CFillCommand(QWidget *parent = 0);
    ~CFillCommand();

public:
	QString m_strCommand;

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::CFillCommand *ui;
};

#endif // FILLCOMMAND_H

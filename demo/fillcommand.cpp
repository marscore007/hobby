#include "fillcommand.h"
#include "ui_fillcommand.h"

CFillCommand::CFillCommand(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CFillCommand)
{
    ui->setupUi(this);
}

CFillCommand::~CFillCommand()
{
    delete ui;
}


void CFillCommand::on_buttonBox_rejected()
{
	
}

void CFillCommand::on_buttonBox_accepted()
{
	m_strCommand = ui->textEdit->toPlainText();
}


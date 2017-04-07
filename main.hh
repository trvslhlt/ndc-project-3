#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

#include "netsocket.hh"

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
	void setupNet();

public slots:
	void gotReturnPressed();
	void processPendingDatagrams();

private:
	QTextEdit *textview;
	QLineEdit *textline;
	NetSocket *sock;
	void sendDatagram(QString text);
};

#endif // P2PAPP_MAIN_HH

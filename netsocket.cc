#include "netsocket.hh"
#include <unistd.h>
#include <sys/types.h>
NetSocket::NetSocket() {
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

bool NetSocket::bind() {
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {

			//Setup all the basic information when bind to the port
			myPortNo = p;
			if (myPortNo - 1 >= myPortMin) {
				myNeighbors.append(myPortNo - 1);
				myNeighborsStatus[QString::number(myPortNo - 1)] = 0;
			}
			if (myPortNo + 1 <= myPortMax) {
				myNeighbors.append(myPortNo + 1);
				myNeighborsStatus[QString::number(myPortNo + 1)] = 0;
				qDebug() << QString::number(myPortNo + 1);
			}
			QHostInfo info;
			originName.append(info.localDomainName());
			originName.append("_");
			originName.append(QString::number(myPortNo));
			originName.append("_");
			originName.append(QUuid::createUuid().toString());
			seqNo = 1;

			qDebug() << "bound to UDP port " << p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

AntientropyTimer::AntientropyTimer(int interval_) {
	timer = new QTimer;
	timer->setInterval(interval_);
	connect(timer, SIGNAL(timeout()), this, SLOT(didTimeoutAntientropy()));
	timer->start();
}

void AntientropyTimer::stop() {
	timer->stop();
}

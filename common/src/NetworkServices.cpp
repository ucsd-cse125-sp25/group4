#include "NetworkServices.h"

int NetworkServices::sendMessage(SOCKET curSocket, char* message, int messageSize) {
	return send(curSocket, message, messageSize, 0);
}

int NetworkServices::recvMessage(SOCKET curSocket, char* buffer, int bufSize) {
	return recv(curSocket, buffer, bufSize, 0);
}

int NetworkServices::recvAll(SOCKET curSocket, char* buffer, int n) {
	int total = 0;
	while (total < n) {
		int r = recvMessage(curSocket, buffer + total, n - total);
		if (r <= 0) return r;
		total += r;
	}
	return total;
}

bool NetworkServices::checkMessage(SOCKET curSocket) {
	if (curSocket == INVALID_SOCKET) {
		return false;
	}

	WSAPOLLFD fd;
	fd.fd = curSocket;
	fd.events = POLLRDNORM;
	fd.revents = 0;

	int ready = WSAPoll(&fd, 1, 0);
	return ready > 0 && (fd.revents & POLLRDNORM);
}
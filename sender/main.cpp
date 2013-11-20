#include <opencv2/highgui/highgui.hpp>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <vector>
#include "coder.h"
#include "io.h"

using namespace std;
using namespace cv;

vector<int> lst;
bool ready = false;

void onMouse(int event, int x, int y, int flags, void*) {
	if (event != EVENT_LBUTTONDOWN) return;
	ready = true;
}

uchar *read_file(const char *filename, int &size, int &num_packets) {
	FILE *fd = fopen(filename, "rb");
	
	fseek(fd, 0, SEEK_END);
	size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	num_packets = (int) ceil((double) size / MAX_DATA);
	uchar *buf =  new uchar[MAX_DATA * num_packets];
	memset(buf, 0, MAX_DATA * num_packets);
	
	for (int i = 0; i != num_packets; ++i) {
		fread(buf + (i * MAX_DATA), sizeof(uchar), MAX_DATA, fd);
	}

	fclose(fd);
	return buf;
}

void connect(IOController &controller, short num_packets, int size) {
	frame frame_a, frame_b;
	frame_a.type = frame_type::INIT;
	frame_a.seq = num_packets;
	*(int *)frame_a.data = size;
	frame_b.type = frame_type::INIT;
	controller.send(frame_a, frame_b);

	int counter = 0;
	time_t past[20], now;
	char text[20];
	memset(past, 0, sizeof(past));
	while (!ready) {
		controller.receive(frame_a, frame_b);
		if (frame_a.type == frame_type::INIT && frame_b.type == frame_type::INIT) {
			time(&now);
			sprintf(text, "%.2f fps, %d", 20 / difftime(now, past[counter]), counter);
			controller.showmsg(text);
			past[counter] = now;
			if (++counter == 20) {
				counter = 0;
			}
		}
		waitKey(10);
	}
}

bool send(IOController &controller, uchar *data) {
	if (lst.empty()) return false;

	frame frame_a, frame_b;

	for (vector<int>::iterator p = lst.begin(); p != lst.end(); ++p) {
		frame_a.type = frame_type::DATA;
		frame_a.seq = *p;
		memcpy(frame_a.data, data + *p * MAX_DATA, MAX_DATA);

		if (++p == lst.end()) {
			frame_b.type = frame_type::END;
			controller.send(frame_a, frame_b);
			return true;
		} else {
			frame_b.type = frame_type::DATA;
			frame_b.seq = *p;
			memcpy(frame_b.data, data + *p * MAX_DATA, MAX_DATA);
			controller.send(frame_a, frame_b);
		}

		waitKey(100);
	}
	
	frame_a.type = frame_type::END;
	frame_b.type = frame_type::END;
	controller.send(frame_a, frame_b);
	waitKey(1);

	return true;
}

void setList(frame &f) {
	short len = f.seq;
	lst.clear();
	for (int i = 0; i != len; ++i) {
		lst.push_back(*((short *) f.data + i));
	}
}

int main() {
	int size, num_packets;
	uchar *data = read_file("msys.ico", size, num_packets);

	IOController controller(640, 480);
	setMouseCallback("w", onMouse);
	connect(controller, num_packets, size);

	controller.showmsg("Sending");

	time_t start, end;
	time(&start);

	for (int i = 0; i != num_packets; ++i)
		lst.push_back(i);

	frame frame_a, frame_b;

	while (true) {
		if (!send(controller, data)) break;
		while (true) {
			controller.receive(frame_a, frame_b);
			if (frame_a.type == frame_type::ACK) {
				setList(frame_a);
				break;
			} else if (frame_b.type == frame_type::ACK) {
				setList(frame_b);
				break;
			}
		}
	}
	
	time(&end);
	controller.showtime(difftime(end, start));
	waitKey();

	delete[] data;
	return 0;
}
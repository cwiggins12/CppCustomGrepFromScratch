#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <chrono>
using namespace std;
using namespace std::chrono;

class Buffer {
private:
	queue<tuple<string, string, int>> buff;
	mutex mtx;
	condition_variable notEmpty;
	condition_variable notFull;
	int buffSize;

public:
	Buffer(int size) : buffSize(size) {}

	void add(tuple<string, string, int> item) {
		unique_lock<mutex> lock(mtx);
		while (buff.size() == buffSize) {
			notFull.wait(lock);
		}
		buff.push(item);
		lock.unlock();
		notEmpty.notify_one();
	}

	void remove(tuple<string, string, int>& item) {
		unique_lock<mutex> lock(mtx);
		while (buff.size() == 0) {
			notEmpty.wait(lock);
		}
		item = buff.front();
		buff.pop();
		lock.unlock();
		notFull.notify_one();
	}
};

bool isFileText(const string& filename) {
	ifstream file(filename, ios::binary);
	if (!file.is_open()) {
		return false;
	}
	char c;
	int count = 0;
	while (file.get(c) && count < 512) {
		if (c == '\0') {
			return false;
		}
		if (!isprint(c) && !isspace(c)) {
			return false;
		}
		count++;
	}
	return true;
}

void traverseDirectory(const string& path, Buffer& buff1) {
	DIR* dir = opendir(path.c_str());
	if (dir == nullptr) {
		cerr << "Unable to open directory: " << path << endl;
		return;
	}
	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		string name = entry->d_name;
		if (name == "." || name == "..") {
			continue;
		}
		string fullPath = path + "/" + name;
		if (entry->d_type == DT_REG) {
			buff1.add(make_tuple("", fullPath, -1));
		}
		else if (entry->d_type == DT_DIR) {
			traverseDirectory(fullPath, buff1);
		}
	}
	closedir(dir);
}

void stage1(Buffer& buff1, steady_clock::time_point& start, steady_clock::time_point& end) {
	start = steady_clock::now();
	traverseDirectory(".", buff1);
	buff1.add(make_tuple("", "", -5));
	end = steady_clock::now();
}

void stage2(int fileSize, int uid, int gid, Buffer& buff1, Buffer& buff2, steady_clock::time_point& start, steady_clock::time_point& end) {
	start = steady_clock::now();
	tuple<string, string, int> item;
	while (true) {
		buff1.remove(item);
		if (item == make_tuple("", "", -5)) { 
			break; 
		}
		struct stat statBuf;
		if (stat(get<1>(item).c_str(), &statBuf) < 0) {
			cerr << "Unable to get stats of " << get<1>(item) << endl;
			continue;
		}
		if ((fileSize == -1 || statBuf.st_size > fileSize) && (uid == -1 || statBuf.st_uid == uid) &&
			(gid == -1 || statBuf.st_gid == gid)) {
			buff2.add(item);
		}
	}
	buff2.add(make_tuple("", "", -5));
	end = steady_clock::now();
}

void stage3(Buffer& buff2, Buffer& buff3, steady_clock::time_point& start, steady_clock::time_point& end) {
	start = steady_clock::now();
	tuple<string, string, int> item;
	while (true) {
		buff2.remove(item);
		if (item == make_tuple("", "", -5)) { 
			break; 
		}
		if (!isFileText(get<1>(item))) {
			continue;
		}
		ifstream file(get<1>(item));
		if (!file.is_open()) {
			cerr << "Unable to open file " << get<1>(item) << endl;
			continue;
		}
		string line;
		int lineNo = 0;
		while (getline(file, line)) {
			lineNo++;
			buff3.add(make_tuple(line, get<1>(item), lineNo));
		}
	}
	buff3.add(make_tuple("", "", -5));
	end = steady_clock::now();
}

void stage4(string word, Buffer& buff3, Buffer& buff4, steady_clock::time_point& start, steady_clock::time_point& end) {
	start = steady_clock::now();
	tuple<string, string, int> item;
	while (true) {
		buff3.remove(item);
		if (item == make_tuple("", "", -5)) { 
			break; 
		}
		if (get<0>(item).find(word) != string::npos) {
			string result = get<1>(item) + "(" + to_string(get<2>(item)) + "): " + get<0>(item);
			buff4.add(make_tuple(result, "", -1));
		}
	}
	buff4.add(make_tuple("", "", -5));
	end = steady_clock::now();
}

void stage5(Buffer& buff4, steady_clock::time_point& start, steady_clock::time_point& end) {
	start = steady_clock::now();
	tuple<string, string, int> item;
	int matchCount = 0;
	while (true) {
		buff4.remove(item);
		if (item == make_tuple("", "", -5)) { 
			break; 
		}
		cout << get<0>(item) << endl;
		matchCount++;
	}
	cout << "***** Found " << matchCount << " matches *****" << endl;
	end = steady_clock::now();
}

void validateArgs(int argc, char* argv[]) {
	if (argc != 6) {
		cerr << "Usage: pipegrep <buffsize> <filesize> <uid> <gid> <word>" << endl;
		exit(1);
	}
	try {
		int buffsize = stoi(argv[1]);
		if (buffsize == 0 || buffsize < -1) {
			cerr << "Error: Buffer size must be -1 or more and nonzero" << endl;
			exit(1);
		}
	}
	catch (const invalid_argument& e) {
		cerr << "Error: Invalid argument - " << e.what() << endl;
		exit(1);
	}
	try {
		int filesize = stoi(argv[2]);
		if (filesize < -1) {
			cerr << "Error: File size must be -1 or more" << endl;
			exit(1);
		}
	}
	catch (const invalid_argument& e) {
		cerr << "Error: Invalid argument - " << e.what() << endl;
		exit(1);
	}
	try {
		int uid = stoi(argv[3]);
		if (uid < -1) {
			cerr << "Error: UID must be -1 or more" << endl;
			exit(1);
		}
	}
	catch (const invalid_argument& e) {
		cerr << "Error: Invalid argument - " << e.what() << endl;
		exit(1);
	}
	try {
		int gid = stoi(argv[4]);
		if (gid < -1) {
			cerr << "Error: GID must be -1 or more" << endl;
			exit(1);
		}
	}
	catch (const invalid_argument& e) {
		cerr << "Error: Invalid argument - " << e.what() << endl;
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	auto programStart = steady_clock::now();
	validateArgs(argc, argv);

	int buffSize = stoi(argv[1]);
	int fileSize = stoi(argv[2]);
	int uid = stoi(argv[3]);
	int gid = stoi(argv[4]);
	string word = argv[5];

	Buffer buff1(buffSize);
	Buffer buff2(buffSize);
	Buffer buff3(buffSize);
	Buffer buff4(buffSize);

	steady_clock::time_point stage1Start, stage1End;
	steady_clock::time_point stage2Start, stage2End;
	steady_clock::time_point stage3Start, stage3End;
	steady_clock::time_point stage4Start, stage4End;
	steady_clock::time_point stage5Start, stage5End;

	vector<thread> stages(5);
	stages[0] = thread(stage1, ref(buff1), ref(stage1Start), ref(stage1End));
	stages[1] = thread(stage2, fileSize, uid, gid, ref(buff1), ref(buff2), ref(stage2Start), ref(stage2End));
	stages[2] = thread(stage3, ref(buff2), ref(buff3), ref(stage3Start), ref(stage3End));
	stages[3] = thread(stage4, word, ref(buff3), ref(buff4), ref(stage4Start), ref(stage4End));
	stages[4] = thread(stage5, ref(buff4), ref(stage5Start), ref(stage5End));

	for (auto& t : stages) {
		t.join();
	}

	auto programEnd = steady_clock::now();
	auto programTime = duration_cast<milliseconds>(programEnd - programStart).count();

	cout << "Stage 1 time: " << duration_cast<milliseconds>(stage1End - stage1Start).count() << " ms" << endl;
	cout << "Stage 2 time: " << duration_cast<milliseconds>(stage2End - stage2Start).count() << " ms" << endl;
	cout << "Stage 3 time: " << duration_cast<milliseconds>(stage3End - stage3Start).count() << " ms" << endl;
	cout << "Stage 4 time: " << duration_cast<milliseconds>(stage4End - stage4Start).count() << " ms" << endl;
	cout << "Stage 5 time: " << duration_cast<milliseconds>(stage5End - stage5Start).count() << " ms" << endl;
	cout << "Program time: " << programTime << " ms" << endl;

	return 0;
}
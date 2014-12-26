
#include <list>
#include <memory>

using namespace ::std;

namespace myantlr{

class Recognizer{
public:
	void reset();
	ssize_t feed(const string &input, ssize_t pos);
protected:
	Recognizer();
	virtual void _reset()=0;
	virtual ssize_t _feed(const string &input, ssize_t pos)=0;
	ssize_t mMatchCount;
};

class CharRecognizer : public Recognizer{
public:
	CharRecognizer(char to_recognize);
	
private:
	virtual void _reset();
	virtual ssize_t _feed(const string &input, ssize_t pos);
	const char mToRecognize;
};

class Selector : public Recognizer{
public:
	Selector();
	void addRecognizer(const shared_ptr<Recognizer> &element);
private:
	virtual void _reset();
	virtual ssize_t _feed(const string &input, ssize_t pos);
	list<shared_ptr<Recognizer>> mElements;
};

}

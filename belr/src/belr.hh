
#include <list>
#include <map>
#include <memory>

using namespace ::std;

namespace belr{

class Recognizer{
public:
	void setName(const string &name);
	void reset();
	size_t feed(const string &input, size_t pos);
protected:
	Recognizer();
	virtual void _reset()=0;
	virtual size_t _feed(const string &input, size_t pos)=0;
	string mName;
};

class CharRecognizer : public Recognizer{
public:
	CharRecognizer(char to_recognize);
	
private:
	virtual void _reset();
	virtual size_t _feed(const string &input, size_t pos);
	const char mToRecognize;
};

class Selector : public Recognizer, public enable_shared_from_this<Selector>{
public:
	Selector();
	shared_ptr<Selector> addRecognizer(const shared_ptr<Recognizer> &element);
private:
	virtual void _reset();
	virtual size_t _feed(const string &input, size_t pos);
	list<shared_ptr<Recognizer>> mElements;
};

class Sequence : public Recognizer, public enable_shared_from_this<Sequence>{
public:
	Sequence();
	shared_ptr<Sequence> addRecognizer(const shared_ptr<Recognizer> &element);
private:
	virtual void _reset();
	virtual size_t _feed(const string &input, size_t pos);
	list<shared_ptr<Recognizer>> mElements;
};

class Loop : public Recognizer, public enable_shared_from_this<Loop>{
public:
	Loop();
	shared_ptr<Loop> setRecognizer(const shared_ptr<Recognizer> &element, int min=0, int max=-1);
private:
	virtual void _reset();
	virtual size_t _feed(const string &input, size_t pos);
	shared_ptr<Recognizer> mRecognizer;
	int mMin, mMax;
};


class Foundation{
public:
	static shared_ptr<CharRecognizer> charRecognizer(char character);
	static shared_ptr<Selector> selector();
	static shared_ptr<Sequence> sequence();
	static shared_ptr<Loop> loop();
};

class Utils{
public:
	static shared_ptr<Recognizer> literal(const string & lt);
	static shared_ptr<Recognizer> char_range(int begin, int end);
};

class RecognizerPointer :  public Recognizer{
public:
	RecognizerPointer();
	shared_ptr<Recognizer> getPointed();
	void setPointed(const shared_ptr<Recognizer> &r);
private:
	virtual void _reset();
	virtual size_t _feed(const string &input, size_t pos);
	shared_ptr<Recognizer> mRecognizer;
};

class Grammar{
public:
	Grammar(const string &name);
	void include(const shared_ptr<Grammar>& grammar);
	template <typename _recognizerT>
	shared_ptr<_recognizerT> addRule(const string & name, const shared_ptr<_recognizerT> &rule){
		assignRule(name, rule);
		return rule;
	}
	shared_ptr<Recognizer> getRule(const string &name);
	bool isComplete()const;
private:
	void assignRule(const string &name, const shared_ptr<Recognizer> &rule);
	map<string,shared_ptr<Recognizer>> mRules;
	string toLower(const string &str);
	string mName;
};

}

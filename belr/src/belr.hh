#ifndef belr_hh
#define belr_hh


#include <list>
#include <map>
#include <memory>

using namespace ::std;

namespace belr{
	
string tolower(const string &str);

class ParserContextBase;

class Recognizer : public enable_shared_from_this<Recognizer>{
public:
	void setName(const string &name);
	const string &getName()const;
	size_t feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
protected:
	Recognizer();
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos)=0;
	string mName;
};

class CharRecognizer : public Recognizer{
public:
	CharRecognizer(int to_recognize, bool caseSensitive=false);
private:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	int mToRecognize;
	bool mCaseSensitive;
};

/*this is an optimization of a selector with multiple individual char recognizer*/
class CharRange : public Recognizer{
public:
	CharRange(int begin, int end);
private:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	int mBegin,mEnd;
};

class Selector : public Recognizer{
public:
	Selector();
	shared_ptr<Selector> addRecognizer(const shared_ptr<Recognizer> &element);
protected:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	list<shared_ptr<Recognizer>> mElements;
};

/**This is an optimization of the first one for the case where there can be only a single match*/
class ExclusiveSelector : public Selector{
public:
	ExclusiveSelector();
private:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
};

class Sequence : public Recognizer{
public:
	Sequence();
	shared_ptr<Sequence> addRecognizer(const shared_ptr<Recognizer> &element);
private:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	list<shared_ptr<Recognizer>> mElements;
};

class Loop : public Recognizer{
public:
	Loop();
	shared_ptr<Loop> setRecognizer(const shared_ptr<Recognizer> &element, int min=0, int max=-1);
private:
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	shared_ptr<Recognizer> mRecognizer;
	int mMin, mMax;
};


class Foundation{
public:
	static shared_ptr<CharRecognizer> charRecognizer(int character, bool caseSensitive=false);
	static shared_ptr<Selector> selector(bool isExclusive=false);
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
	virtual size_t _feed(const shared_ptr<ParserContextBase> &ctx, const string &input, size_t pos);
	shared_ptr<Recognizer> mRecognizer;
};

class Grammar{
public:
	Grammar(const string &name);
	void include(const shared_ptr<Grammar>& grammar);
	/* the grammar takes ownership of the recognizer, which must not be used outside of this grammar.
	 * TODO: use unique_ptr to enforce this, or make a copy ?
	**/
	template <typename _recognizerT>
	shared_ptr<_recognizerT> addRule(const string & name, const shared_ptr<_recognizerT> &rule){
		assignRule(name, rule);
		return rule;
	}
	template <typename _recognizerT>
	shared_ptr<_recognizerT> extendRule(const string & name, const shared_ptr<_recognizerT> &rule){
		_extendRule(name, rule);
		return rule;
	}
	shared_ptr<Recognizer> getRule(const string &name);
	bool isComplete()const;
private:
	void assignRule(const string &name, const shared_ptr<Recognizer> &rule);
	void _extendRule(const string &name, const shared_ptr<Recognizer> &rule);
	map<string,shared_ptr<Recognizer>> mRules;
	string mName;
};




}

#endif

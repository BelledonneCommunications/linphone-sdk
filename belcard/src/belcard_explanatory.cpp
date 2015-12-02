#include "belcard/belcard_explanatory.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardCategories> BelCardCategories::create() {
	return BelCardGeneric::create<BelCardCategories>();
}

shared_ptr<BelCardCategories> BelCardCategories::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("CATEGORIES", input, NULL);
	return dynamic_pointer_cast<BelCardCategories>(ret);
}

void BelCardCategories::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CATEGORIES", make_fn(&BelCardCategories::create))
			->setCollector("group", make_sfn(&BelCardCategories::setGroup))
			->setCollector("any-param", make_sfn(&BelCardCategories::addParam))
			->setCollector("CATEGORIES-value", make_sfn(&BelCardCategories::setValue));
}

BelCardCategories::BelCardCategories() : BelCardProperty() {
	setName("CATEGORIES");
}

shared_ptr<BelCardNote> BelCardNote::create() {
	return BelCardGeneric::create<BelCardNote>();
}

shared_ptr<BelCardNote> BelCardNote::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("NOTE", input, NULL);
	return dynamic_pointer_cast<BelCardNote>(ret);
}

void BelCardNote::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("NOTE", make_fn(&BelCardNote::create))
			->setCollector("group", make_sfn(&BelCardNote::setGroup))
			->setCollector("any-param", make_sfn(&BelCardNote::addParam))
			->setCollector("NOTE-value", make_sfn(&BelCardNote::setValue));
}

BelCardNote::BelCardNote() : BelCardProperty() {
	setName("NOTE");
}

shared_ptr<BelCardProductId> BelCardProductId::create() {
	return BelCardGeneric::create<BelCardProductId>();
}

shared_ptr<BelCardProductId> BelCardProductId::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("PRODID", input, NULL);
	return dynamic_pointer_cast<BelCardProductId>(ret);
}

void BelCardProductId::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("PRODID", make_fn(&BelCardProductId::create))
			->setCollector("group", make_sfn(&BelCardProductId::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProductId::addParam))
			->setCollector("PRODID-value", make_sfn(&BelCardProductId::setValue));
}

BelCardProductId::BelCardProductId() : BelCardProperty() {
	setName("PRODID");
}

shared_ptr<BelCardRevision> BelCardRevision::create() {
	return BelCardGeneric::create<BelCardRevision>();
}

shared_ptr<BelCardRevision> BelCardRevision::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("REV", input, NULL);
	return dynamic_pointer_cast<BelCardRevision>(ret);
}

void BelCardRevision::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("REV", make_fn(&BelCardRevision::create))
			->setCollector("group", make_sfn(&BelCardRevision::setGroup))
			->setCollector("any-param", make_sfn(&BelCardRevision::addParam))
			->setCollector("REV-value", make_sfn(&BelCardRevision::setValue));
}

BelCardRevision::BelCardRevision() : BelCardProperty() {
	setName("REV");
}

shared_ptr<BelCardSound> BelCardSound::create() {
	return BelCardGeneric::create<BelCardSound>();
}

shared_ptr<BelCardSound> BelCardSound::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("SOUND", input, NULL);
	return dynamic_pointer_cast<BelCardSound>(ret);
}

void BelCardSound::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("SOUND", make_fn(&BelCardSound::create))
			->setCollector("group", make_sfn(&BelCardSound::setGroup))
			->setCollector("any-param", make_sfn(&BelCardSound::addParam))
			->setCollector("SOUND-value", make_sfn(&BelCardSound::setValue));
}

BelCardSound::BelCardSound() : BelCardProperty() {
	setName("SOUND");
}

shared_ptr<BelCardUniqueId> BelCardUniqueId::create() {
	return BelCardGeneric::create<BelCardUniqueId>();
}

shared_ptr<BelCardUniqueId> BelCardUniqueId::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("UID", input, NULL);
	return dynamic_pointer_cast<BelCardUniqueId>(ret);
}

void BelCardUniqueId::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("UID", make_fn(&BelCardUniqueId::create))
			->setCollector("group", make_sfn(&BelCardUniqueId::setGroup))
			->setCollector("any-param", make_sfn(&BelCardUniqueId::addParam))
			->setCollector("UID-value", make_sfn(&BelCardUniqueId::setValue));
}

BelCardUniqueId::BelCardUniqueId() : BelCardProperty() {
	setName("UID");
}

shared_ptr<BelCardClientProductIdMap> BelCardClientProductIdMap::create() {
	return BelCardGeneric::create<BelCardClientProductIdMap>();
}

shared_ptr<BelCardClientProductIdMap> BelCardClientProductIdMap::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("CLIENTPIDMAP", input, NULL);
	return dynamic_pointer_cast<BelCardClientProductIdMap>(ret);
}

void BelCardClientProductIdMap::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CLIENTPIDMAP", make_fn(&BelCardClientProductIdMap::create))
			->setCollector("group", make_sfn(&BelCardClientProductIdMap::setGroup))
			->setCollector("any-param", make_sfn(&BelCardClientProductIdMap::addParam))
			->setCollector("CLIENTPIDMAP-value", make_sfn(&BelCardClientProductIdMap::setValue));
}

BelCardClientProductIdMap::BelCardClientProductIdMap() : BelCardProperty() {
	setName("CLIENTPIDMAP");
}

shared_ptr<BelCardURL> BelCardURL::create() {
	return BelCardGeneric::create<BelCardURL>();
}

shared_ptr<BelCardURL> BelCardURL::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("URL", input, NULL);
	return dynamic_pointer_cast<BelCardURL>(ret);
}

void BelCardURL::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("URL", make_fn(&BelCardURL::create))
			->setCollector("group", make_sfn(&BelCardURL::setGroup))
			->setCollector("any-param", make_sfn(&BelCardURL::addParam))
			->setCollector("URL-value", make_sfn(&BelCardURL::setValue));
}

BelCardURL::BelCardURL() : BelCardProperty() {
	setName("URL");
}
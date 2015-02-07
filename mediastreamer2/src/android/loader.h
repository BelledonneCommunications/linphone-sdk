

#ifndef ms_loader_h
#define ms_loader_h


#include <cstdlib>


class Library{
public:
	static Library *load(const char *path);
	void *getSymbol(const char *name);
private:
	Library(void*);
	void *mHandle;
};



class FunctionBase{
	public:
		FunctionBase(Library *lib, const char *symbol_name);
		bool load(Library *lib, const char *symbol_name);
		bool isFound()const{
			return mSymbolAddr!=NULL;
		}
	protected:
		void *mSymbolAddr;
};

template <typename _retT>
class Function0 : public FunctionBase{
	public:
		typedef _retT (*proto_t)();
		Function0(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)();
			}
			return 0;
		}
};

template <>
class Function0<void>: public FunctionBase{
	public:
		typedef void (*proto_t)();
		Function0(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)();
			}
		}
};

template <typename _retT, typename _arg0T>
class Function1 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T arg0);
		Function1(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0);
			}
			return 0;
		}
};

template <typename _arg0T>
class Function1<void,_arg0T>: public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T);
		Function1(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T>
class Function2 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T ,_arg1T);
		Function2(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T>
class Function2<void,_arg0T,_arg1T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T arg0,_arg1T);
		Function2(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T>
class Function3 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T);
		Function3(Library *lib, const char *mangled_name): FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T>
class Function3<void,_arg0T,_arg1T,_arg2T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T, _arg2T);
		Function3(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T>
class Function4 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T);
		Function4(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T>
class Function4<void,_arg0T,_arg1T,_arg2T,_arg3T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T, _arg2T, _arg3T);
		Function4(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T>
class Function5 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T);
		Function5(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T>
class Function5<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T);
		Function5(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4);
			}
		}
};


template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T>
class Function6 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T);
		Function6(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T>
class Function6<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T);
		Function6(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T, typename _arg6T>
class Function7 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T);
		Function7(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T, typename _arg6T>
class Function7<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T);
		Function7(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T>
class Function8 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T);
		Function8(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T>
class Function8<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T);
		Function8(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7);
			}
		}
};


template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T>
class Function9 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T);
		Function9(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T >
class Function9<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T);
		Function9(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T>
class Function10 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T);
		Function10(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T>
class Function10<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T);
		Function10(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T>
class Function11 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T);
		Function11(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T>
class Function11<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T);
		Function11(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T, typename _arg11T>
class Function12 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T, _arg11T);
		Function12(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10, _arg11T arg11){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10, arg11);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T,  typename _arg11T>
class Function12<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T, _arg11T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T, _arg11T);
		Function12(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10, _arg11T arg11){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T, typename _arg11T, typename _arg12T>
class Function13 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T, _arg11T, _arg12T);
		Function13(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10, _arg11T arg11, _arg12T arg12){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T,  typename _arg11T, typename _arg12T>
class Function13<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T, _arg11T, _arg12T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T, _arg11T, _arg12T);
		Function13(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10, _arg11T arg11, _arg12T arg12){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T, typename _arg11T, typename _arg12T, typename _arg13T>
class Function14 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T, _arg11T, _arg12T, _arg13T);
		Function14(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T,  typename _arg11T, typename _arg12T,typename _arg13T>
class Function14<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T, _arg11T, _arg12T, _arg13T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T, _arg11T, _arg12T, _arg13T);
		Function14(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T, typename _arg11T, typename _arg12T, typename _arg13T, typename _arg14T>
class Function15 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T, _arg11T, _arg12T, _arg13T,_arg14T);
		Function15(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13, _arg14T arg14){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T,  typename _arg11T, typename _arg12T,typename _arg13T, typename _arg14T>
class Function15<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T, _arg11T, _arg12T, _arg13T,_arg14T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T, _arg11T, _arg12T, _arg13T, _arg14T);
		Function15(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13, _arg14T arg14){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14);
			}
		}
};

template <typename _retT, typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T, typename _arg9T, typename _arg10T, typename _arg11T, typename _arg12T, typename _arg13T, typename _arg14T, typename _arg15T>
class Function16 : public FunctionBase{
	public:
		typedef _retT (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T,_arg10T, _arg11T, _arg12T, _arg13T,_arg14T,_arg15T);
		Function16(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		_retT invoke(_arg0T arg0, _arg1T arg1, _arg2T arg2,_arg3T arg3, _arg4T arg4, _arg5T arg5, _arg6T arg6,_arg7T arg7,_arg8T arg8, _arg9T arg9, _arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13, _arg14T arg14, _arg15T arg15){
			if (mSymbolAddr){
				return ((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14,arg15);
			}
			return 0;
		}
};

template <typename _arg0T, typename _arg1T, typename _arg2T, typename _arg3T, 
typename _arg4T, typename _arg5T, typename _arg6T, typename _arg7T, typename _arg8T , typename _arg9T, typename _arg10T,  typename _arg11T, typename _arg12T,typename _arg13T, typename _arg14T, typename _arg15T>
class Function16<void,_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T,_arg9T, _arg10T, _arg11T, _arg12T, _arg13T,_arg14T,_arg15T> : public FunctionBase{
	public:
		typedef void (*proto_t)(_arg0T,_arg1T,_arg2T,_arg3T,_arg4T,_arg5T,_arg6T,_arg7T,_arg8T, _arg9T, _arg10T, _arg11T, _arg12T, _arg13T, _arg14T, _arg15T);
		Function16(Library *lib, const char *mangled_name) : FunctionBase(lib,mangled_name){};
		void invoke(_arg0T arg0, _arg1T arg1,_arg2T arg2, _arg3T arg3, _arg4T arg4, _arg5T arg5,_arg6T arg6, _arg7T arg7,_arg8T arg8,_arg9T arg9 ,_arg10T arg10, _arg11T arg11, _arg12T arg12, _arg13T arg13, _arg14T arg14, _arg15T arg15){
			if (mSymbolAddr){
				((proto_t)mSymbolAddr)(arg0,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12,arg13,arg14,arg15);
			}
		}
};

#endif

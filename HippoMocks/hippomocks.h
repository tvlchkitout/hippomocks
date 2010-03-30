#ifndef HIPPOMOCKS_H
#define HIPPOMOCKS_H

#ifndef EXCEPTION_BUFFER_SIZE
#define EXCEPTION_BUFFER_SIZE 65536
#endif

#ifndef BASE_EXCEPTION
#define BASE_EXCEPTION std::exception
#include <exception>
#endif

#ifndef DONTCARE_NAME
#define DONTCARE_NAME _
#endif

#ifndef VIRT_FUNC_LIMIT
#define VIRT_FUNC_LIMIT 1024
#elif VIRT_FUNC_LIMIT > 1024
#error Adjust the code to support more than 1024 virtual functions before setting the VIRT_FUNC_LIMIT above 1024
#endif

#ifdef __GNUC__
#define EXTRA_DESTRUCTOR
#endif

#ifdef __EDG__
#define FUNCTION_BASE 3
#define FUNCTION_STRIDE 2
#else
#define FUNCTION_BASE 0
#define FUNCTION_STRIDE 1
#endif

#include <list>
#include <map>
#include <memory>
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef LINUX_TARGET
#include <execinfo.h>
#endif

#ifdef _MSC_VER
// these warnings are pointless and huge, and will confuse new users.
#pragma warning(push)
// If you can't generate an assignment operator the least you can do is shut up.
#pragma warning(disable: 4512)
// Alignment not right in a union?
#pragma warning(disable: 4121)
// No deprecated warnings on functions that really aren't deprecated at all.
#pragma warning(disable: 4996)

// Tell Microsoft to conform to C++ (as far as is possible...)
#pragma pointers_to_members(full_generality, virtual_inheritance)
#endif

#include <memory.h>

#define REPLACE(x, y) Replace __replace_obj_##x (&x, &y)

#ifdef _MSC_VER
#include <windows.h>

class Unprotect
{
public:
  Unprotect(void *location, size_t byteCount)
  : origFunc(location)
  , byteCount(byteCount)
  {
    VirtualProtect(origFunc, byteCount, PAGE_EXECUTE_READWRITE, &oldprotect); 
  }
  ~Unprotect()
  {
    unsigned long dontcare;
    VirtualProtect(origFunc, byteCount, oldprotect, &dontcare);
  }
private:
  void *origFunc;
  size_t byteCount;
  unsigned long oldprotect;
};
#else
#include <sys/mman.h>
#include <stdint.h>

class Unprotect
{
public:
  Unprotect(void *location, size_t byteCount)
  : origFunc((intptr_t)location & (~0xFFF))
  , byteCount(byteCount + ((intptr_t)location - origFunc))
  {
    mprotect((void *)origFunc, byteCount, PROT_READ|PROT_WRITE|PROT_EXEC); 
  };
  ~Unprotect()
  {
    mprotect((void *)origFunc, byteCount, PROT_READ|PROT_EXEC); 
  }
private:
  intptr_t origFunc;
  int byteCount;
};
#endif

typedef unsigned int e9ptrsize_t;

template <typename T, typename U>
T horrible_cast(U u)
{
	union { T t; U u; } un;
	un.u = u;
	return un.t;
}

class Replace
{
private:
  void *origFunc;
  char backupData[sizeof(e9ptrsize_t) + 1];
public:
  template <typename T>
  Replace(T funcptr, T replacement)
      : origFunc(horrible_cast<void *>(funcptr))
  {
    Unprotect _allow_write(origFunc, sizeof(e9ptrsize_t) + 1);
    memcpy(backupData, origFunc, sizeof(backupData));
    *(unsigned char *)origFunc = 0xE9;
    *(e9ptrsize_t*)(horrible_cast<intptr_t>(origFunc) + 1) = (e9ptrsize_t)(horrible_cast<intptr_t>(replacement) - horrible_cast<intptr_t>(origFunc) - sizeof(e9ptrsize_t) - 1);
  }
  ~Replace()
  {
    Unprotect _allow_write(origFunc, sizeof(e9ptrsize_t) + 1);
    memcpy(origFunc, backupData, sizeof(backupData)); 
  }
};

class MockRepository;

enum RegistrationType {
	Never,
	Once,
	Any
};

// base type
class base_mock {
public:
	void destroy() { unwriteVft(); delete this; }
	virtual ~base_mock() {}
	void *rewriteVft(void *newVf)
	{
		void *oldVf = *(void **)this;
		*(void **)this = newVf;
		return oldVf;
	}
	void reset()
	{
		unwriteVft();
		mock_reset();
	}
	virtual void mock_reset() = 0;
	void unwriteVft()
	{
		*(void **)this = (*(void ***)this)[VIRT_FUNC_LIMIT+1];
	}
};

class NullType
{
public:
	bool operator==(const NullType &) const
	{
		return true;
	}
};

class DontCare {};
static DontCare DONTCARE_NAME;
static void _notused() {
	if (&DONTCARE_NAME > 0) return;
	_notused();
}

template <typename T>
struct OutParam: public DontCare
{
	explicit OutParam(T value): value(value) {}
	T value;
};

template <typename T>
OutParam<T> Out(T t) { return OutParam<T>(t); }

template <typename T, bool isPointer>
struct InParam;

template <typename T>
struct InParam<T, false>: public DontCare
{
	explicit InParam(T& value): value(value)
	{
	}
	T& value;
};

template <typename T>
struct InParam<T, true>: public DontCare
{
	explicit InParam(T*& value): value(value)
	{
	}
	T*& value;
};

template <typename T>
InParam<T, false> In(T& t) { return InParam<T, false>(t); }

template <typename T>
InParam<T, true> In(T*& t) { return InParam<T, true>(t); }

struct NotPrintable { template <typename T> NotPrintable(T const&) {} };

inline std::ostream &operator<<(std::ostream &os, NotPrintable const&)
{
	os << "???";
	return os;
}

inline std::ostream &operator<<(std::ostream &os, DontCare const&)
{
	os << "_";
	return os;
}

template <typename T>
struct printArg
{
	static inline void print(std::ostream &os, T arg, bool withComma)
	{
		if (withComma)
			os << ",";
		os << arg;
	}
};

template <>
struct printArg<NullType>
{
	static void print(std::ostream &, NullType , bool)
	{
	}
};

class base_tuple
{
protected:
	base_tuple()
	{
	}
public:
	virtual ~base_tuple()
	{
	}
	virtual void printTo(std::ostream &os) const = 0;
};

template <typename X>
struct no_cref { typedef X type; };

template <typename X>
struct no_cref<const X &> { typedef X type; };

template <typename A> struct with_const { typedef const A type; };
template <typename A> struct with_const<A &> { typedef const A &type; };
template <typename A> struct with_const<const A> { typedef const A type; };
template <typename A> struct with_const<const A &> { typedef const A &type; };

template <typename T> struct base_type { typedef T type; };
template <typename T> struct base_type<T&> { typedef T type; };
template <typename T> struct base_type<const T> { typedef T type; };
template <typename T> struct base_type<const T&> { typedef T type; };

template <typename T>
struct comparer
{
	static inline bool compare(typename with_const<T>::type a, typename with_const<T>::type b)
	{
		return a == b;
	}
	static inline bool compare(DontCare, typename with_const<T>::type)
	{
		return true;
	}
};

template <typename T>
struct IsOutParamType { enum { value = false }; };
template <typename T>
struct IsOutParamType<OutParam<T> > { enum { value = true }; };

template <typename T>
struct IsInParamType { enum { value = false }; };
template <typename T>
struct IsInParamType<InParam<T, true> > { enum { value = true }; };
template <typename T>
struct IsInParamType<InParam<T, false> > { enum { value = true }; };

template <typename T1, typename T2, bool Assign>
struct do_assign;

template <typename T1, typename T2>
struct do_assign<T1, T2*, true>
{
  static void assign_to(T1 outparam, T2 *refparam)
  {
    *refparam = outparam.value;
  }
  static void assign_from(T1 inparam, T2 *refparam)
  {
    inparam.value = refparam;
  }
};

template <typename T1, typename T2>
struct do_assign<T1, T2&, true>
{
  static void assign_to(T1 outparam, T2 &refparam)
  {
    refparam = outparam.value;
  }
  static void assign_from(T1 inparam, T2 &refparam)
  {
    inparam.value = refparam;
  }
};

template <typename T1, typename T2>
struct do_assign<T1, T2, false>
{
	static void assign_to(T1, T2) {}
	static void assign_from(T1, T2) {}
};

template <typename T1, typename T2>
void out_assign(T1 a, T2 b)
{
	do_assign<T1, T2, IsOutParamType<typename base_type<T1>::type>::value >::assign_to(a, b);
}

template <typename T1, typename T2>
void in_assign(T1 a, T2 b)
{
	do_assign<T1, T2, IsInParamType<typename base_type<T1>::type>::value >::assign_from(a, b);
}

template <typename A = NullType, typename B = NullType, typename C = NullType, typename D = NullType,
		  typename E = NullType, typename F = NullType, typename G = NullType, typename H = NullType,
		  typename I = NullType, typename J = NullType, typename K = NullType, typename L = NullType,
		  typename M = NullType, typename N = NullType, typename O = NullType, typename P = NullType>
class ref_tuple : public base_tuple
{
public:
	A a;
	B b;
	C c;
	D d;
	E e;
	F f;
	G g;
	H h;
	I i;
	J j;
	K k;
	L l;
	M m;
	N n;
	O o;
	P p;
	ref_tuple(A a = A(), B b = B(), C c = C(), D d = D(), E e = E(), F f = F(), G g = G(), H h = H(), I i = I(), J j = J(), K k = K(), L l = L(), M m = M(), N n = N(), O o = O(), P p = P())
		  : a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), i(i), j(j), k(k), l(l), m(m), n(n), o(o), p(p)
	{}
	virtual void printTo(std::ostream &os) const
	{
	  os << "(";
	  printArg<A>::print(os, a, false);
	  printArg<B>::print(os, b, true);
	  printArg<C>::print(os, c, true);
	  printArg<D>::print(os, d, true);
	  printArg<E>::print(os, e, true);
	  printArg<F>::print(os, f, true);
	  printArg<G>::print(os, g, true);
	  printArg<H>::print(os, h, true);
	  printArg<I>::print(os, i, true);
	  printArg<J>::print(os, j, true);
	  printArg<K>::print(os, k, true);
	  printArg<L>::print(os, l, true);
	  printArg<M>::print(os, m, true);
	  printArg<N>::print(os, n, true);
	  printArg<O>::print(os, o, true);
	  printArg<P>::print(os, p, true);
	  os << ")";
	}
};

template <typename A = NullType, typename B = NullType, typename C = NullType, typename D = NullType,
		  typename E = NullType, typename F = NullType, typename G = NullType, typename H = NullType,
		  typename I = NullType, typename J = NullType, typename K = NullType, typename L = NullType,
		  typename M = NullType, typename N = NullType, typename O = NullType, typename P = NullType>
class ref_comparable_assignable_tuple : public base_tuple
{
public:
	virtual bool operator==(const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &bo) = 0;
	virtual void assign_from(ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &from) = 0;
  virtual void assign_to(ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &to) = 0;
};

template <typename A, typename B>
struct with_ref
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<A, B&>
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<A&, B>
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<A&, B&>
{
	typedef const B & type;
};

template <typename A, typename B>
struct with_ref<A, const B&>
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<A, const OutParam<B> &>
{
  typedef OutParam<B> type;
};

template <typename A, typename B>
struct with_ref<A&, const B&>
{
	typedef const B & type;
};

template <typename A, typename B>
struct with_ref<A&, const OutParam<B> &>
{
  typedef OutParam<B> type;
};

template <typename A, typename B>
struct with_ref<const A&, B>
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<const A&, B&>
{
	typedef B type;
};

template <typename A, typename B>
struct with_ref<const A&, const B&>
{
	typedef B type;
};

// Fix specifically for char arrays (which aren't assignable, but don't change either)
template <typename A, int B>
struct with_ref<A, const char (&)[B]>
{
	typedef const char *type;
};
template <typename A, int B>
struct with_ref<A&, const char (&)[B]>
{
	typedef const char *type;
};
template <typename A, int B>
struct with_ref<const A, const char (&)[B]>
{
	typedef const char *type;
};
template <typename A, int B>
struct with_ref<const A&, const char (&)[B]>
{
	typedef const char *type;
};

template <typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P,
		  typename CA, typename CB, typename CC, typename CD, typename CE, typename CF, typename CG, typename CH,
		  typename CI, typename CJ, typename CK, typename CL, typename CM, typename CN, typename CO, typename CP>
class copy_tuple : public ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>
{
public:
	typename with_ref<A,CA>::type a;
	typename with_ref<B,CB>::type b;
	typename with_ref<C,CC>::type c;
	typename with_ref<D,CD>::type d;
	typename with_ref<E,CE>::type e;
	typename with_ref<F,CF>::type f;
	typename with_ref<G,CG>::type g;
	typename with_ref<H,CH>::type h;
	typename with_ref<I,CI>::type i;
	typename with_ref<J,CJ>::type j;
	typename with_ref<K,CK>::type k;
	typename with_ref<L,CL>::type l;
	typename with_ref<M,CM>::type m;
	typename with_ref<N,CN>::type n;
	typename with_ref<O,CO>::type o;
	typename with_ref<P,CP>::type p;
	copy_tuple(typename with_ref<A,CA>::type a, typename with_ref<B,CB>::type b,
		typename with_ref<C,CC>::type c, typename with_ref<D,CD>::type d,
		typename with_ref<E,CE>::type e, typename with_ref<F,CF>::type f,
		typename with_ref<G,CG>::type g, typename with_ref<H,CH>::type h,
		typename with_ref<I,CI>::type i, typename with_ref<J,CJ>::type j,
		typename with_ref<K,CK>::type k, typename with_ref<L,CL>::type l,
		typename with_ref<M,CM>::type m, typename with_ref<N,CN>::type n,
		typename with_ref<O,CO>::type o, typename with_ref<P,CP>::type p)
		  : a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), i(i), j(j), k(k), l(l), m(m), n(n), o(o), p(p)
	{}
	bool operator==(const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &to)
	{
		return (comparer<A>::compare(a, to.a) &&
				comparer<B>::compare(b, to.b) &&
				comparer<C>::compare(c, to.c) &&
				comparer<D>::compare(d, to.d) &&
				comparer<E>::compare(e, to.e) &&
				comparer<F>::compare(f, to.f) &&
				comparer<G>::compare(g, to.g) &&
				comparer<H>::compare(h, to.h) &&
				comparer<I>::compare(i, to.i) &&
				comparer<J>::compare(j, to.j) &&
				comparer<K>::compare(k, to.k) &&
				comparer<L>::compare(l, to.l) &&
				comparer<M>::compare(m, to.m) &&
				comparer<N>::compare(n, to.n) &&
				comparer<O>::compare(o, to.o) &&
				comparer<P>::compare(p, to.p));
	}
	void assign_from(ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &from)
	{
		in_assign< typename with_ref<A,CA>::type, A>(a, from.a);
		in_assign< typename with_ref<B,CB>::type, B>(b, from.b);
		in_assign< typename with_ref<C,CC>::type, C>(c, from.c);
		in_assign< typename with_ref<D,CD>::type, D>(d, from.d);
		in_assign< typename with_ref<E,CE>::type, E>(e, from.e);
		in_assign< typename with_ref<F,CF>::type, F>(f, from.f);
		in_assign< typename with_ref<G,CG>::type, G>(g, from.g);
		in_assign< typename with_ref<H,CH>::type, H>(h, from.h);
		in_assign< typename with_ref<I,CI>::type, I>(i, from.i);
		in_assign< typename with_ref<J,CJ>::type, J>(j, from.j);
		in_assign< typename with_ref<K,CK>::type, K>(k, from.k);
		in_assign< typename with_ref<L,CL>::type, L>(l, from.l);
		in_assign< typename with_ref<M,CM>::type, M>(m, from.m);
		in_assign< typename with_ref<N,CN>::type, N>(n, from.n);
		in_assign< typename with_ref<O,CO>::type, O>(o, from.o);
		in_assign< typename with_ref<P,CP>::type, P>(p, from.p);
	}
	void assign_to(ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &to)
	{
		out_assign< typename with_ref<A,CA>::type, A>(a, to.a);
		out_assign< typename with_ref<B,CB>::type, B>(b, to.b);
		out_assign< typename with_ref<C,CC>::type, C>(c, to.c);
		out_assign< typename with_ref<D,CD>::type, D>(d, to.d);
		out_assign< typename with_ref<E,CE>::type, E>(e, to.e);
		out_assign< typename with_ref<F,CF>::type, F>(f, to.f);
		out_assign< typename with_ref<G,CG>::type, G>(g, to.g);
		out_assign< typename with_ref<H,CH>::type, H>(h, to.h);
		out_assign< typename with_ref<I,CI>::type, I>(i, to.i);
		out_assign< typename with_ref<J,CJ>::type, J>(j, to.j);
		out_assign< typename with_ref<K,CK>::type, K>(k, to.k);
		out_assign< typename with_ref<L,CL>::type, L>(l, to.l);
		out_assign< typename with_ref<M,CM>::type, M>(m, to.m);
		out_assign< typename with_ref<N,CN>::type, N>(n, to.n);
		out_assign< typename with_ref<O,CO>::type, O>(o, to.o);
		out_assign< typename with_ref<P,CP>::type, P>(p, to.p);
	}
	virtual void printTo(std::ostream &os) const
	{
		os << "(";
		printArg<typename with_ref<A,CA>::type>::print(os, a, false);
		printArg<typename with_ref<B,CB>::type>::print(os, b, true);
		printArg<typename with_ref<C,CC>::type>::print(os, c, true);
		printArg<typename with_ref<D,CD>::type>::print(os, d, true);
		printArg<typename with_ref<E,CE>::type>::print(os, e, true);
		printArg<typename with_ref<F,CF>::type>::print(os, f, true);
		printArg<typename with_ref<G,CG>::type>::print(os, g, true);
		printArg<typename with_ref<H,CH>::type>::print(os, h, true);
		printArg<typename with_ref<I,CI>::type>::print(os, i, true);
		printArg<typename with_ref<J,CJ>::type>::print(os, j, true);
		printArg<typename with_ref<K,CK>::type>::print(os, k, true);
		printArg<typename with_ref<L,CL>::type>::print(os, l, true);
		printArg<typename with_ref<M,CM>::type>::print(os, m, true);
		printArg<typename with_ref<N,CN>::type>::print(os, n, true);
		printArg<typename with_ref<O,CO>::type>::print(os, o, true);
		printArg<typename with_ref<P,CP>::type>::print(os, p, true);
		os << ")";
	}
};

inline std::ostream &operator<<(std::ostream &os, const MockRepository &repo);

class BaseException : public BASE_EXCEPTION {
public:
	~BaseException() throw() {}
	const char *what() const throw() { return txt.c_str(); }
protected:
	std::string txt;
};

// exception types
class ExpectationException : public BaseException {
public:
	ExpectationException(MockRepository *repo, const base_tuple *tuple, const char *funcName)
	{
		std::stringstream text;
		text << "Function ";
		text << funcName;
		if (tuple)
			tuple->printTo(text);
		else
			text << "(...)";
		text << " called with mismatching expectation!" << std::endl;
		text << *repo;
		txt = text.str();
	}
};

class NotImplementedException : public BaseException {
public:
	NotImplementedException(MockRepository *repo)
	{
		std::stringstream text;
		text << "Function called without expectation!" << std::endl;
		text << *repo;

#ifdef LINUX_TARGET
		void* stacktrace[256];
		size_t size = backtrace( stacktrace, sizeof(stacktrace) );
		if( size > 0 )
		{
			text << "Stackdump:" << std::endl;
			char **symbols = backtrace_symbols( stacktrace, size );
			for( size_t i = 0; i < size; i = i + 1 )
			{
				text << symbols[i] << std::endl;
			}
			free( symbols );
		}
#endif

		txt = text.str();
	}
};

class CallMissingException : public BaseException {
public:
	CallMissingException(MockRepository *repo)
	{
		std::stringstream text;
		text << "Function with expectation not called!" << std::endl;
		text << *repo;
		txt = text.str();
	}
};

class ZombieMockException : public BaseException {
public:
	ZombieMockException(MockRepository *repo)
	{
		std::stringstream text;
		text << "Function called on mock that has already been destroyed!" << std::endl;
		text << *repo;

#ifdef LINUX_TARGET
    void* stacktrace[256];
    size_t size = backtrace( stacktrace, sizeof(stacktrace) );
    if( size > 0 )
    {
      text << "Stackdump:" << std::endl;
      char **symbols = backtrace_symbols( stacktrace, size );
      for( size_t i = 0; i < size; i = i + 1 )
      {
        text << symbols[i] << std::endl;
      }
      free( symbols );
    }
#endif

		txt = text.str();
	}
};

class NoResultSetUpException : public BaseException {
public:
	NoResultSetUpException(MockRepository *repo, const base_tuple *tuple, const char *funcName)
	{
		std::stringstream text;
		text << "No result set up on call to ";
		text << funcName;
		if (tuple)
			tuple->printTo(text);
		else
			text << "(...)";
		text << std::endl;
		text << *repo;
		txt = text.str();
	}
};

// function-index-of-type
class func_index {
public:
	int lci;
	virtual int f0(int) { return lci=0; }			virtual int f1(int) { return lci=1; }			virtual int f2(int) { return lci=2; }			virtual int f3(int) { return lci=3; }
	virtual int f4(int) { return lci=4; }			virtual int f5(int) { return lci=5; }			virtual int f6(int) { return lci=6; }			virtual int f7(int) { return lci=7; }
	virtual int f8(int) { return lci=8; }			virtual int f9(int) { return lci=9; }			virtual int f10(int) { return lci=10; }		virtual int f11(int) { return lci=11; }
	virtual int f12(int) { return lci=12; }		virtual int f13(int) { return lci=13; }		virtual int f14(int) { return lci=14; }		virtual int f15(int) { return lci=15; }
	virtual int f16(int) { return lci=16; }		virtual int f17(int) { return lci=17; }		virtual int f18(int) { return lci=18; }		virtual int f19(int) { return lci=19; }
	virtual int f20(int) { return lci=20; }		virtual int f21(int) { return lci=21; }		virtual int f22(int) { return lci=22; }		virtual int f23(int) { return lci=23; }
	virtual int f24(int) { return lci=24; }		virtual int f25(int) { return lci=25; }		virtual int f26(int) { return lci=26; }		virtual int f27(int) { return lci=27; }
	virtual int f28(int) { return lci=28; }		virtual int f29(int) { return lci=29; }		virtual int f30(int) { return lci=30; }		virtual int f31(int) { return lci=31; }
	virtual int f32(int) { return lci=32; }		virtual int f33(int) { return lci=33; }		virtual int f34(int) { return lci=34; }		virtual int f35(int) { return lci=35; }
	virtual int f36(int) { return lci=36; }		virtual int f37(int) { return lci=37; }		virtual int f38(int) { return lci=38; }		virtual int f39(int) { return lci=39; }
	virtual int f40(int) { return lci=40; }		virtual int f41(int) { return lci=41; }		virtual int f42(int) { return lci=42; }		virtual int f43(int) { return lci=43; }
	virtual int f44(int) { return lci=44; }		virtual int f45(int) { return lci=45; }		virtual int f46(int) { return lci=46; }		virtual int f47(int) { return lci=47; }
	virtual int f48(int) { return lci=48; }		virtual int f49(int) { return lci=49; }		virtual int f50(int) { return lci=50; }		virtual int f51(int) { return lci=51; }
	virtual int f52(int) { return lci=52; }		virtual int f53(int) { return lci=53; }		virtual int f54(int) { return lci=54; }		virtual int f55(int) { return lci=55; }
	virtual int f56(int) { return lci=56; }		virtual int f57(int) { return lci=57; }		virtual int f58(int) { return lci=58; }		virtual int f59(int) { return lci=59; }
	virtual int f60(int) { return lci=60; }		virtual int f61(int) { return lci=61; }		virtual int f62(int) { return lci=62; }		virtual int f63(int) { return lci=63; }
	virtual int f64(int) { return lci=64; }		virtual int f65(int) { return lci=65; }		virtual int f66(int) { return lci=66; }		virtual int f67(int) { return lci=67; }
	virtual int f68(int) { return lci=68; }		virtual int f69(int) { return lci=69; }		virtual int f70(int) { return lci=70; }		virtual int f71(int) { return lci=71; }
	virtual int f72(int) { return lci=72; }		virtual int f73(int) { return lci=73; }		virtual int f74(int) { return lci=74; }		virtual int f75(int) { return lci=75; }
	virtual int f76(int) { return lci=76; }		virtual int f77(int) { return lci=77; }		virtual int f78(int) { return lci=78; }		virtual int f79(int) { return lci=79; }
	virtual int f80(int) { return lci=80; }		virtual int f81(int) { return lci=81; }		virtual int f82(int) { return lci=82; }		virtual int f83(int) { return lci=83; }
	virtual int f84(int) { return lci=84; }		virtual int f85(int) { return lci=85; }		virtual int f86(int) { return lci=86; }		virtual int f87(int) { return lci=87; }
	virtual int f88(int) { return lci=88; }		virtual int f89(int) { return lci=89; }		virtual int f90(int) { return lci=90; }		virtual int f91(int) { return lci=91; }
	virtual int f92(int) { return lci=92; }		virtual int f93(int) { return lci=93; }		virtual int f94(int) { return lci=94; }		virtual int f95(int) { return lci=95; }
	virtual int f96(int) { return lci=96; }		virtual int f97(int) { return lci=97; }		virtual int f98(int) { return lci=98; }		virtual int f99(int) { return lci=99; }
	virtual int f100(int) { return lci=100; }		virtual int f101(int) { return lci=101; }		virtual int f102(int) { return lci=102; }		virtual int f103(int) { return lci=103; }
	virtual int f104(int) { return lci=104; }		virtual int f105(int) { return lci=105; }		virtual int f106(int) { return lci=106; }		virtual int f107(int) { return lci=107; }
	virtual int f108(int) { return lci=108; }		virtual int f109(int) { return lci=109; }		virtual int f110(int) { return lci=110; }		virtual int f111(int) { return lci=111; }
	virtual int f112(int) { return lci=112; }		virtual int f113(int) { return lci=113; }		virtual int f114(int) { return lci=114; }		virtual int f115(int) { return lci=115; }
	virtual int f116(int) { return lci=116; }		virtual int f117(int) { return lci=117; }		virtual int f118(int) { return lci=118; }		virtual int f119(int) { return lci=119; }
	virtual int f120(int) { return lci=120; }		virtual int f121(int) { return lci=121; }		virtual int f122(int) { return lci=122; }		virtual int f123(int) { return lci=123; }
	virtual int f124(int) { return lci=124; }		virtual int f125(int) { return lci=125; }		virtual int f126(int) { return lci=126; }		virtual int f127(int) { return lci=127; }
	virtual int f128(int) { return lci=128; }		virtual int f129(int) { return lci=129; }		virtual int f130(int) { return lci=130; }		virtual int f131(int) { return lci=131; }
	virtual int f132(int) { return lci=132; }		virtual int f133(int) { return lci=133; }		virtual int f134(int) { return lci=134; }		virtual int f135(int) { return lci=135; }
	virtual int f136(int) { return lci=136; }		virtual int f137(int) { return lci=137; }		virtual int f138(int) { return lci=138; }		virtual int f139(int) { return lci=139; }
	virtual int f140(int) { return lci=140; }		virtual int f141(int) { return lci=141; }		virtual int f142(int) { return lci=142; }		virtual int f143(int) { return lci=143; }
	virtual int f144(int) { return lci=144; }		virtual int f145(int) { return lci=145; }		virtual int f146(int) { return lci=146; }		virtual int f147(int) { return lci=147; }
	virtual int f148(int) { return lci=148; }		virtual int f149(int) { return lci=149; }		virtual int f150(int) { return lci=150; }		virtual int f151(int) { return lci=151; }
	virtual int f152(int) { return lci=152; }		virtual int f153(int) { return lci=153; }		virtual int f154(int) { return lci=154; }		virtual int f155(int) { return lci=155; }
	virtual int f156(int) { return lci=156; }		virtual int f157(int) { return lci=157; }		virtual int f158(int) { return lci=158; }		virtual int f159(int) { return lci=159; }
	virtual int f160(int) { return lci=160; }		virtual int f161(int) { return lci=161; }		virtual int f162(int) { return lci=162; }		virtual int f163(int) { return lci=163; }
	virtual int f164(int) { return lci=164; }		virtual int f165(int) { return lci=165; }		virtual int f166(int) { return lci=166; }		virtual int f167(int) { return lci=167; }
	virtual int f168(int) { return lci=168; }		virtual int f169(int) { return lci=169; }		virtual int f170(int) { return lci=170; }		virtual int f171(int) { return lci=171; }
	virtual int f172(int) { return lci=172; }		virtual int f173(int) { return lci=173; }		virtual int f174(int) { return lci=174; }		virtual int f175(int) { return lci=175; }
	virtual int f176(int) { return lci=176; }		virtual int f177(int) { return lci=177; }		virtual int f178(int) { return lci=178; }		virtual int f179(int) { return lci=179; }
	virtual int f180(int) { return lci=180; }		virtual int f181(int) { return lci=181; }		virtual int f182(int) { return lci=182; }		virtual int f183(int) { return lci=183; }
	virtual int f184(int) { return lci=184; }		virtual int f185(int) { return lci=185; }		virtual int f186(int) { return lci=186; }		virtual int f187(int) { return lci=187; }
	virtual int f188(int) { return lci=188; }		virtual int f189(int) { return lci=189; }		virtual int f190(int) { return lci=190; }		virtual int f191(int) { return lci=191; }
	virtual int f192(int) { return lci=192; }		virtual int f193(int) { return lci=193; }		virtual int f194(int) { return lci=194; }		virtual int f195(int) { return lci=195; }
	virtual int f196(int) { return lci=196; }		virtual int f197(int) { return lci=197; }		virtual int f198(int) { return lci=198; }		virtual int f199(int) { return lci=199; }
	virtual int f200(int) { return lci=200; }		virtual int f201(int) { return lci=201; }		virtual int f202(int) { return lci=202; }		virtual int f203(int) { return lci=203; }
	virtual int f204(int) { return lci=204; }		virtual int f205(int) { return lci=205; }		virtual int f206(int) { return lci=206; }		virtual int f207(int) { return lci=207; }
	virtual int f208(int) { return lci=208; }		virtual int f209(int) { return lci=209; }		virtual int f210(int) { return lci=210; }		virtual int f211(int) { return lci=211; }
	virtual int f212(int) { return lci=212; }		virtual int f213(int) { return lci=213; }		virtual int f214(int) { return lci=214; }		virtual int f215(int) { return lci=215; }
	virtual int f216(int) { return lci=216; }		virtual int f217(int) { return lci=217; }		virtual int f218(int) { return lci=218; }		virtual int f219(int) { return lci=219; }
	virtual int f220(int) { return lci=220; }		virtual int f221(int) { return lci=221; }		virtual int f222(int) { return lci=222; }		virtual int f223(int) { return lci=223; }
	virtual int f224(int) { return lci=224; }		virtual int f225(int) { return lci=225; }		virtual int f226(int) { return lci=226; }		virtual int f227(int) { return lci=227; }
	virtual int f228(int) { return lci=228; }		virtual int f229(int) { return lci=229; }		virtual int f230(int) { return lci=230; }		virtual int f231(int) { return lci=231; }
	virtual int f232(int) { return lci=232; }		virtual int f233(int) { return lci=233; }		virtual int f234(int) { return lci=234; }		virtual int f235(int) { return lci=235; }
	virtual int f236(int) { return lci=236; }		virtual int f237(int) { return lci=237; }		virtual int f238(int) { return lci=238; }		virtual int f239(int) { return lci=239; }
	virtual int f240(int) { return lci=240; }		virtual int f241(int) { return lci=241; }		virtual int f242(int) { return lci=242; }		virtual int f243(int) { return lci=243; }
	virtual int f244(int) { return lci=244; }		virtual int f245(int) { return lci=245; }		virtual int f246(int) { return lci=246; }		virtual int f247(int) { return lci=247; }
	virtual int f248(int) { return lci=248; }		virtual int f249(int) { return lci=249; }		virtual int f250(int) { return lci=250; }		virtual int f251(int) { return lci=251; }
	virtual int f252(int) { return lci=252; }		virtual int f253(int) { return lci=253; }		virtual int f254(int) { return lci=254; }		virtual int f255(int) { return lci=255; }
	virtual int f256(int) { return lci=256; }		virtual int f257(int) { return lci=257; }		virtual int f258(int) { return lci=258; }		virtual int f259(int) { return lci=259; }
	virtual int f260(int) { return lci=260; }		virtual int f261(int) { return lci=261; }		virtual int f262(int) { return lci=262; }		virtual int f263(int) { return lci=263; }
	virtual int f264(int) { return lci=264; }		virtual int f265(int) { return lci=265; }		virtual int f266(int) { return lci=266; }		virtual int f267(int) { return lci=267; }
	virtual int f268(int) { return lci=268; }		virtual int f269(int) { return lci=269; }		virtual int f270(int) { return lci=270; }		virtual int f271(int) { return lci=271; }
	virtual int f272(int) { return lci=272; }		virtual int f273(int) { return lci=273; }		virtual int f274(int) { return lci=274; }		virtual int f275(int) { return lci=275; }
	virtual int f276(int) { return lci=276; }		virtual int f277(int) { return lci=277; }		virtual int f278(int) { return lci=278; }		virtual int f279(int) { return lci=279; }
	virtual int f280(int) { return lci=280; }		virtual int f281(int) { return lci=281; }		virtual int f282(int) { return lci=282; }		virtual int f283(int) { return lci=283; }
	virtual int f284(int) { return lci=284; }		virtual int f285(int) { return lci=285; }		virtual int f286(int) { return lci=286; }		virtual int f287(int) { return lci=287; }
	virtual int f288(int) { return lci=288; }		virtual int f289(int) { return lci=289; }		virtual int f290(int) { return lci=290; }		virtual int f291(int) { return lci=291; }
	virtual int f292(int) { return lci=292; }		virtual int f293(int) { return lci=293; }		virtual int f294(int) { return lci=294; }		virtual int f295(int) { return lci=295; }
	virtual int f296(int) { return lci=296; }		virtual int f297(int) { return lci=297; }		virtual int f298(int) { return lci=298; }		virtual int f299(int) { return lci=299; }
	virtual int f300(int) { return lci=300; }		virtual int f301(int) { return lci=301; }		virtual int f302(int) { return lci=302; }		virtual int f303(int) { return lci=303; }
	virtual int f304(int) { return lci=304; }		virtual int f305(int) { return lci=305; }		virtual int f306(int) { return lci=306; }		virtual int f307(int) { return lci=307; }
	virtual int f308(int) { return lci=308; }		virtual int f309(int) { return lci=309; }		virtual int f310(int) { return lci=310; }		virtual int f311(int) { return lci=311; }
	virtual int f312(int) { return lci=312; }		virtual int f313(int) { return lci=313; }		virtual int f314(int) { return lci=314; }		virtual int f315(int) { return lci=315; }
	virtual int f316(int) { return lci=316; }		virtual int f317(int) { return lci=317; }		virtual int f318(int) { return lci=318; }		virtual int f319(int) { return lci=319; }
	virtual int f320(int) { return lci=320; }		virtual int f321(int) { return lci=321; }		virtual int f322(int) { return lci=322; }		virtual int f323(int) { return lci=323; }
	virtual int f324(int) { return lci=324; }		virtual int f325(int) { return lci=325; }		virtual int f326(int) { return lci=326; }		virtual int f327(int) { return lci=327; }
	virtual int f328(int) { return lci=328; }		virtual int f329(int) { return lci=329; }		virtual int f330(int) { return lci=330; }		virtual int f331(int) { return lci=331; }
	virtual int f332(int) { return lci=332; }		virtual int f333(int) { return lci=333; }		virtual int f334(int) { return lci=334; }		virtual int f335(int) { return lci=335; }
	virtual int f336(int) { return lci=336; }		virtual int f337(int) { return lci=337; }		virtual int f338(int) { return lci=338; }		virtual int f339(int) { return lci=339; }
	virtual int f340(int) { return lci=340; }		virtual int f341(int) { return lci=341; }		virtual int f342(int) { return lci=342; }		virtual int f343(int) { return lci=343; }
	virtual int f344(int) { return lci=344; }		virtual int f345(int) { return lci=345; }		virtual int f346(int) { return lci=346; }		virtual int f347(int) { return lci=347; }
	virtual int f348(int) { return lci=348; }		virtual int f349(int) { return lci=349; }		virtual int f350(int) { return lci=350; }		virtual int f351(int) { return lci=351; }
	virtual int f352(int) { return lci=352; }		virtual int f353(int) { return lci=353; }		virtual int f354(int) { return lci=354; }		virtual int f355(int) { return lci=355; }
	virtual int f356(int) { return lci=356; }		virtual int f357(int) { return lci=357; }		virtual int f358(int) { return lci=358; }		virtual int f359(int) { return lci=359; }
	virtual int f360(int) { return lci=360; }		virtual int f361(int) { return lci=361; }		virtual int f362(int) { return lci=362; }		virtual int f363(int) { return lci=363; }
	virtual int f364(int) { return lci=364; }		virtual int f365(int) { return lci=365; }		virtual int f366(int) { return lci=366; }		virtual int f367(int) { return lci=367; }
	virtual int f368(int) { return lci=368; }		virtual int f369(int) { return lci=369; }		virtual int f370(int) { return lci=370; }		virtual int f371(int) { return lci=371; }
	virtual int f372(int) { return lci=372; }		virtual int f373(int) { return lci=373; }		virtual int f374(int) { return lci=374; }		virtual int f375(int) { return lci=375; }
	virtual int f376(int) { return lci=376; }		virtual int f377(int) { return lci=377; }		virtual int f378(int) { return lci=378; }		virtual int f379(int) { return lci=379; }
	virtual int f380(int) { return lci=380; }		virtual int f381(int) { return lci=381; }		virtual int f382(int) { return lci=382; }		virtual int f383(int) { return lci=383; }
	virtual int f384(int) { return lci=384; }		virtual int f385(int) { return lci=385; }		virtual int f386(int) { return lci=386; }		virtual int f387(int) { return lci=387; }
	virtual int f388(int) { return lci=388; }		virtual int f389(int) { return lci=389; }		virtual int f390(int) { return lci=390; }		virtual int f391(int) { return lci=391; }
	virtual int f392(int) { return lci=392; }		virtual int f393(int) { return lci=393; }		virtual int f394(int) { return lci=394; }		virtual int f395(int) { return lci=395; }
	virtual int f396(int) { return lci=396; }		virtual int f397(int) { return lci=397; }		virtual int f398(int) { return lci=398; }		virtual int f399(int) { return lci=399; }
	virtual int f400(int) { return lci=400; }		virtual int f401(int) { return lci=401; }		virtual int f402(int) { return lci=402; }		virtual int f403(int) { return lci=403; }
	virtual int f404(int) { return lci=404; }		virtual int f405(int) { return lci=405; }		virtual int f406(int) { return lci=406; }		virtual int f407(int) { return lci=407; }
	virtual int f408(int) { return lci=408; }		virtual int f409(int) { return lci=409; }		virtual int f410(int) { return lci=410; }		virtual int f411(int) { return lci=411; }
	virtual int f412(int) { return lci=412; }		virtual int f413(int) { return lci=413; }		virtual int f414(int) { return lci=414; }		virtual int f415(int) { return lci=415; }
	virtual int f416(int) { return lci=416; }		virtual int f417(int) { return lci=417; }		virtual int f418(int) { return lci=418; }		virtual int f419(int) { return lci=419; }
	virtual int f420(int) { return lci=420; }		virtual int f421(int) { return lci=421; }		virtual int f422(int) { return lci=422; }		virtual int f423(int) { return lci=423; }
	virtual int f424(int) { return lci=424; }		virtual int f425(int) { return lci=425; }		virtual int f426(int) { return lci=426; }		virtual int f427(int) { return lci=427; }
	virtual int f428(int) { return lci=428; }		virtual int f429(int) { return lci=429; }		virtual int f430(int) { return lci=430; }		virtual int f431(int) { return lci=431; }
	virtual int f432(int) { return lci=432; }		virtual int f433(int) { return lci=433; }		virtual int f434(int) { return lci=434; }		virtual int f435(int) { return lci=435; }
	virtual int f436(int) { return lci=436; }		virtual int f437(int) { return lci=437; }		virtual int f438(int) { return lci=438; }		virtual int f439(int) { return lci=439; }
	virtual int f440(int) { return lci=440; }		virtual int f441(int) { return lci=441; }		virtual int f442(int) { return lci=442; }		virtual int f443(int) { return lci=443; }
	virtual int f444(int) { return lci=444; }		virtual int f445(int) { return lci=445; }		virtual int f446(int) { return lci=446; }		virtual int f447(int) { return lci=447; }
	virtual int f448(int) { return lci=448; }		virtual int f449(int) { return lci=449; }		virtual int f450(int) { return lci=450; }		virtual int f451(int) { return lci=451; }
	virtual int f452(int) { return lci=452; }		virtual int f453(int) { return lci=453; }		virtual int f454(int) { return lci=454; }		virtual int f455(int) { return lci=455; }
	virtual int f456(int) { return lci=456; }		virtual int f457(int) { return lci=457; }		virtual int f458(int) { return lci=458; }		virtual int f459(int) { return lci=459; }
	virtual int f460(int) { return lci=460; }		virtual int f461(int) { return lci=461; }		virtual int f462(int) { return lci=462; }		virtual int f463(int) { return lci=463; }
	virtual int f464(int) { return lci=464; }		virtual int f465(int) { return lci=465; }		virtual int f466(int) { return lci=466; }		virtual int f467(int) { return lci=467; }
	virtual int f468(int) { return lci=468; }		virtual int f469(int) { return lci=469; }		virtual int f470(int) { return lci=470; }		virtual int f471(int) { return lci=471; }
	virtual int f472(int) { return lci=472; }		virtual int f473(int) { return lci=473; }		virtual int f474(int) { return lci=474; }		virtual int f475(int) { return lci=475; }
	virtual int f476(int) { return lci=476; }		virtual int f477(int) { return lci=477; }		virtual int f478(int) { return lci=478; }		virtual int f479(int) { return lci=479; }
	virtual int f480(int) { return lci=480; }		virtual int f481(int) { return lci=481; }		virtual int f482(int) { return lci=482; }		virtual int f483(int) { return lci=483; }
	virtual int f484(int) { return lci=484; }		virtual int f485(int) { return lci=485; }		virtual int f486(int) { return lci=486; }		virtual int f487(int) { return lci=487; }
	virtual int f488(int) { return lci=488; }		virtual int f489(int) { return lci=489; }		virtual int f490(int) { return lci=490; }		virtual int f491(int) { return lci=491; }
	virtual int f492(int) { return lci=492; }		virtual int f493(int) { return lci=493; }		virtual int f494(int) { return lci=494; }		virtual int f495(int) { return lci=495; }
	virtual int f496(int) { return lci=496; }		virtual int f497(int) { return lci=497; }		virtual int f498(int) { return lci=498; }		virtual int f499(int) { return lci=499; }
	virtual int f500(int) { return lci=500; }		virtual int f501(int) { return lci=501; }		virtual int f502(int) { return lci=502; }		virtual int f503(int) { return lci=503; }
	virtual int f504(int) { return lci=504; }		virtual int f505(int) { return lci=505; }		virtual int f506(int) { return lci=506; }		virtual int f507(int) { return lci=507; }
	virtual int f508(int) { return lci=508; }		virtual int f509(int) { return lci=509; }		virtual int f510(int) { return lci=510; }		virtual int f511(int) { return lci=511; }
	virtual int f512(int) { return lci=512; }		virtual int f513(int) { return lci=513; }		virtual int f514(int) { return lci=514; }		virtual int f515(int) { return lci=515; }
	virtual int f516(int) { return lci=516; }		virtual int f517(int) { return lci=517; }		virtual int f518(int) { return lci=518; }		virtual int f519(int) { return lci=519; }
	virtual int f520(int) { return lci=520; }		virtual int f521(int) { return lci=521; }		virtual int f522(int) { return lci=522; }		virtual int f523(int) { return lci=523; }
	virtual int f524(int) { return lci=524; }		virtual int f525(int) { return lci=525; }		virtual int f526(int) { return lci=526; }		virtual int f527(int) { return lci=527; }
	virtual int f528(int) { return lci=528; }		virtual int f529(int) { return lci=529; }		virtual int f530(int) { return lci=530; }		virtual int f531(int) { return lci=531; }
	virtual int f532(int) { return lci=532; }		virtual int f533(int) { return lci=533; }		virtual int f534(int) { return lci=534; }		virtual int f535(int) { return lci=535; }
	virtual int f536(int) { return lci=536; }		virtual int f537(int) { return lci=537; }		virtual int f538(int) { return lci=538; }		virtual int f539(int) { return lci=539; }
	virtual int f540(int) { return lci=540; }		virtual int f541(int) { return lci=541; }		virtual int f542(int) { return lci=542; }		virtual int f543(int) { return lci=543; }
	virtual int f544(int) { return lci=544; }		virtual int f545(int) { return lci=545; }		virtual int f546(int) { return lci=546; }		virtual int f547(int) { return lci=547; }
	virtual int f548(int) { return lci=548; }		virtual int f549(int) { return lci=549; }		virtual int f550(int) { return lci=550; }		virtual int f551(int) { return lci=551; }
	virtual int f552(int) { return lci=552; }		virtual int f553(int) { return lci=553; }		virtual int f554(int) { return lci=554; }		virtual int f555(int) { return lci=555; }
	virtual int f556(int) { return lci=556; }		virtual int f557(int) { return lci=557; }		virtual int f558(int) { return lci=558; }		virtual int f559(int) { return lci=559; }
	virtual int f560(int) { return lci=560; }		virtual int f561(int) { return lci=561; }		virtual int f562(int) { return lci=562; }		virtual int f563(int) { return lci=563; }
	virtual int f564(int) { return lci=564; }		virtual int f565(int) { return lci=565; }		virtual int f566(int) { return lci=566; }		virtual int f567(int) { return lci=567; }
	virtual int f568(int) { return lci=568; }		virtual int f569(int) { return lci=569; }		virtual int f570(int) { return lci=570; }		virtual int f571(int) { return lci=571; }
	virtual int f572(int) { return lci=572; }		virtual int f573(int) { return lci=573; }		virtual int f574(int) { return lci=574; }		virtual int f575(int) { return lci=575; }
	virtual int f576(int) { return lci=576; }		virtual int f577(int) { return lci=577; }		virtual int f578(int) { return lci=578; }		virtual int f579(int) { return lci=579; }
	virtual int f580(int) { return lci=580; }		virtual int f581(int) { return lci=581; }		virtual int f582(int) { return lci=582; }		virtual int f583(int) { return lci=583; }
	virtual int f584(int) { return lci=584; }		virtual int f585(int) { return lci=585; }		virtual int f586(int) { return lci=586; }		virtual int f587(int) { return lci=587; }
	virtual int f588(int) { return lci=588; }		virtual int f589(int) { return lci=589; }		virtual int f590(int) { return lci=590; }		virtual int f591(int) { return lci=591; }
	virtual int f592(int) { return lci=592; }		virtual int f593(int) { return lci=593; }		virtual int f594(int) { return lci=594; }		virtual int f595(int) { return lci=595; }
	virtual int f596(int) { return lci=596; }		virtual int f597(int) { return lci=597; }		virtual int f598(int) { return lci=598; }		virtual int f599(int) { return lci=599; }
	virtual int f600(int) { return lci=600; }		virtual int f601(int) { return lci=601; }		virtual int f602(int) { return lci=602; }		virtual int f603(int) { return lci=603; }
	virtual int f604(int) { return lci=604; }		virtual int f605(int) { return lci=605; }		virtual int f606(int) { return lci=606; }		virtual int f607(int) { return lci=607; }
	virtual int f608(int) { return lci=608; }		virtual int f609(int) { return lci=609; }		virtual int f610(int) { return lci=610; }		virtual int f611(int) { return lci=611; }
	virtual int f612(int) { return lci=612; }		virtual int f613(int) { return lci=613; }		virtual int f614(int) { return lci=614; }		virtual int f615(int) { return lci=615; }
	virtual int f616(int) { return lci=616; }		virtual int f617(int) { return lci=617; }		virtual int f618(int) { return lci=618; }		virtual int f619(int) { return lci=619; }
	virtual int f620(int) { return lci=620; }		virtual int f621(int) { return lci=621; }		virtual int f622(int) { return lci=622; }		virtual int f623(int) { return lci=623; }
	virtual int f624(int) { return lci=624; }		virtual int f625(int) { return lci=625; }		virtual int f626(int) { return lci=626; }		virtual int f627(int) { return lci=627; }
	virtual int f628(int) { return lci=628; }		virtual int f629(int) { return lci=629; }		virtual int f630(int) { return lci=630; }		virtual int f631(int) { return lci=631; }
	virtual int f632(int) { return lci=632; }		virtual int f633(int) { return lci=633; }		virtual int f634(int) { return lci=634; }		virtual int f635(int) { return lci=635; }
	virtual int f636(int) { return lci=636; }		virtual int f637(int) { return lci=637; }		virtual int f638(int) { return lci=638; }		virtual int f639(int) { return lci=639; }
	virtual int f640(int) { return lci=640; }		virtual int f641(int) { return lci=641; }		virtual int f642(int) { return lci=642; }		virtual int f643(int) { return lci=643; }
	virtual int f644(int) { return lci=644; }		virtual int f645(int) { return lci=645; }		virtual int f646(int) { return lci=646; }		virtual int f647(int) { return lci=647; }
	virtual int f648(int) { return lci=648; }		virtual int f649(int) { return lci=649; }		virtual int f650(int) { return lci=650; }		virtual int f651(int) { return lci=651; }
	virtual int f652(int) { return lci=652; }		virtual int f653(int) { return lci=653; }		virtual int f654(int) { return lci=654; }		virtual int f655(int) { return lci=655; }
	virtual int f656(int) { return lci=656; }		virtual int f657(int) { return lci=657; }		virtual int f658(int) { return lci=658; }		virtual int f659(int) { return lci=659; }
	virtual int f660(int) { return lci=660; }		virtual int f661(int) { return lci=661; }		virtual int f662(int) { return lci=662; }		virtual int f663(int) { return lci=663; }
	virtual int f664(int) { return lci=664; }		virtual int f665(int) { return lci=665; }		virtual int f666(int) { return lci=666; }		virtual int f667(int) { return lci=667; }
	virtual int f668(int) { return lci=668; }		virtual int f669(int) { return lci=669; }		virtual int f670(int) { return lci=670; }		virtual int f671(int) { return lci=671; }
	virtual int f672(int) { return lci=672; }		virtual int f673(int) { return lci=673; }		virtual int f674(int) { return lci=674; }		virtual int f675(int) { return lci=675; }
	virtual int f676(int) { return lci=676; }		virtual int f677(int) { return lci=677; }		virtual int f678(int) { return lci=678; }		virtual int f679(int) { return lci=679; }
	virtual int f680(int) { return lci=680; }		virtual int f681(int) { return lci=681; }		virtual int f682(int) { return lci=682; }		virtual int f683(int) { return lci=683; }
	virtual int f684(int) { return lci=684; }		virtual int f685(int) { return lci=685; }		virtual int f686(int) { return lci=686; }		virtual int f687(int) { return lci=687; }
	virtual int f688(int) { return lci=688; }		virtual int f689(int) { return lci=689; }		virtual int f690(int) { return lci=690; }		virtual int f691(int) { return lci=691; }
	virtual int f692(int) { return lci=692; }		virtual int f693(int) { return lci=693; }		virtual int f694(int) { return lci=694; }		virtual int f695(int) { return lci=695; }
	virtual int f696(int) { return lci=696; }		virtual int f697(int) { return lci=697; }		virtual int f698(int) { return lci=698; }		virtual int f699(int) { return lci=699; }
	virtual int f700(int) { return lci=700; }		virtual int f701(int) { return lci=701; }		virtual int f702(int) { return lci=702; }		virtual int f703(int) { return lci=703; }
	virtual int f704(int) { return lci=704; }		virtual int f705(int) { return lci=705; }		virtual int f706(int) { return lci=706; }		virtual int f707(int) { return lci=707; }
	virtual int f708(int) { return lci=708; }		virtual int f709(int) { return lci=709; }		virtual int f710(int) { return lci=710; }		virtual int f711(int) { return lci=711; }
	virtual int f712(int) { return lci=712; }		virtual int f713(int) { return lci=713; }		virtual int f714(int) { return lci=714; }		virtual int f715(int) { return lci=715; }
	virtual int f716(int) { return lci=716; }		virtual int f717(int) { return lci=717; }		virtual int f718(int) { return lci=718; }		virtual int f719(int) { return lci=719; }
	virtual int f720(int) { return lci=720; }		virtual int f721(int) { return lci=721; }		virtual int f722(int) { return lci=722; }		virtual int f723(int) { return lci=723; }
	virtual int f724(int) { return lci=724; }		virtual int f725(int) { return lci=725; }		virtual int f726(int) { return lci=726; }		virtual int f727(int) { return lci=727; }
	virtual int f728(int) { return lci=728; }		virtual int f729(int) { return lci=729; }		virtual int f730(int) { return lci=730; }		virtual int f731(int) { return lci=731; }
	virtual int f732(int) { return lci=732; }		virtual int f733(int) { return lci=733; }		virtual int f734(int) { return lci=734; }		virtual int f735(int) { return lci=735; }
	virtual int f736(int) { return lci=736; }		virtual int f737(int) { return lci=737; }		virtual int f738(int) { return lci=738; }		virtual int f739(int) { return lci=739; }
	virtual int f740(int) { return lci=740; }		virtual int f741(int) { return lci=741; }		virtual int f742(int) { return lci=742; }		virtual int f743(int) { return lci=743; }
	virtual int f744(int) { return lci=744; }		virtual int f745(int) { return lci=745; }		virtual int f746(int) { return lci=746; }		virtual int f747(int) { return lci=747; }
	virtual int f748(int) { return lci=748; }		virtual int f749(int) { return lci=749; }		virtual int f750(int) { return lci=750; }		virtual int f751(int) { return lci=751; }
	virtual int f752(int) { return lci=752; }		virtual int f753(int) { return lci=753; }		virtual int f754(int) { return lci=754; }		virtual int f755(int) { return lci=755; }
	virtual int f756(int) { return lci=756; }		virtual int f757(int) { return lci=757; }		virtual int f758(int) { return lci=758; }		virtual int f759(int) { return lci=759; }
	virtual int f760(int) { return lci=760; }		virtual int f761(int) { return lci=761; }		virtual int f762(int) { return lci=762; }		virtual int f763(int) { return lci=763; }
	virtual int f764(int) { return lci=764; }		virtual int f765(int) { return lci=765; }		virtual int f766(int) { return lci=766; }		virtual int f767(int) { return lci=767; }
	virtual int f768(int) { return lci=768; }		virtual int f769(int) { return lci=769; }		virtual int f770(int) { return lci=770; }		virtual int f771(int) { return lci=771; }
	virtual int f772(int) { return lci=772; }		virtual int f773(int) { return lci=773; }		virtual int f774(int) { return lci=774; }		virtual int f775(int) { return lci=775; }
	virtual int f776(int) { return lci=776; }		virtual int f777(int) { return lci=777; }		virtual int f778(int) { return lci=778; }		virtual int f779(int) { return lci=779; }
	virtual int f780(int) { return lci=780; }		virtual int f781(int) { return lci=781; }		virtual int f782(int) { return lci=782; }		virtual int f783(int) { return lci=783; }
	virtual int f784(int) { return lci=784; }		virtual int f785(int) { return lci=785; }		virtual int f786(int) { return lci=786; }		virtual int f787(int) { return lci=787; }
	virtual int f788(int) { return lci=788; }		virtual int f789(int) { return lci=789; }		virtual int f790(int) { return lci=790; }		virtual int f791(int) { return lci=791; }
	virtual int f792(int) { return lci=792; }		virtual int f793(int) { return lci=793; }		virtual int f794(int) { return lci=794; }		virtual int f795(int) { return lci=795; }
	virtual int f796(int) { return lci=796; }		virtual int f797(int) { return lci=797; }		virtual int f798(int) { return lci=798; }		virtual int f799(int) { return lci=799; }
	virtual int f800(int) { return lci=800; }		virtual int f801(int) { return lci=801; }		virtual int f802(int) { return lci=802; }		virtual int f803(int) { return lci=803; }
	virtual int f804(int) { return lci=804; }		virtual int f805(int) { return lci=805; }		virtual int f806(int) { return lci=806; }		virtual int f807(int) { return lci=807; }
	virtual int f808(int) { return lci=808; }		virtual int f809(int) { return lci=809; }		virtual int f810(int) { return lci=810; }		virtual int f811(int) { return lci=811; }
	virtual int f812(int) { return lci=812; }		virtual int f813(int) { return lci=813; }		virtual int f814(int) { return lci=814; }		virtual int f815(int) { return lci=815; }
	virtual int f816(int) { return lci=816; }		virtual int f817(int) { return lci=817; }		virtual int f818(int) { return lci=818; }		virtual int f819(int) { return lci=819; }
	virtual int f820(int) { return lci=820; }		virtual int f821(int) { return lci=821; }		virtual int f822(int) { return lci=822; }		virtual int f823(int) { return lci=823; }
	virtual int f824(int) { return lci=824; }		virtual int f825(int) { return lci=825; }		virtual int f826(int) { return lci=826; }		virtual int f827(int) { return lci=827; }
	virtual int f828(int) { return lci=828; }		virtual int f829(int) { return lci=829; }		virtual int f830(int) { return lci=830; }		virtual int f831(int) { return lci=831; }
	virtual int f832(int) { return lci=832; }		virtual int f833(int) { return lci=833; }		virtual int f834(int) { return lci=834; }		virtual int f835(int) { return lci=835; }
	virtual int f836(int) { return lci=836; }		virtual int f837(int) { return lci=837; }		virtual int f838(int) { return lci=838; }		virtual int f839(int) { return lci=839; }
	virtual int f840(int) { return lci=840; }		virtual int f841(int) { return lci=841; }		virtual int f842(int) { return lci=842; }		virtual int f843(int) { return lci=843; }
	virtual int f844(int) { return lci=844; }		virtual int f845(int) { return lci=845; }		virtual int f846(int) { return lci=846; }		virtual int f847(int) { return lci=847; }
	virtual int f848(int) { return lci=848; }		virtual int f849(int) { return lci=849; }		virtual int f850(int) { return lci=850; }		virtual int f851(int) { return lci=851; }
	virtual int f852(int) { return lci=852; }		virtual int f853(int) { return lci=853; }		virtual int f854(int) { return lci=854; }		virtual int f855(int) { return lci=855; }
	virtual int f856(int) { return lci=856; }		virtual int f857(int) { return lci=857; }		virtual int f858(int) { return lci=858; }		virtual int f859(int) { return lci=859; }
	virtual int f860(int) { return lci=860; }		virtual int f861(int) { return lci=861; }		virtual int f862(int) { return lci=862; }		virtual int f863(int) { return lci=863; }
	virtual int f864(int) { return lci=864; }		virtual int f865(int) { return lci=865; }		virtual int f866(int) { return lci=866; }		virtual int f867(int) { return lci=867; }
	virtual int f868(int) { return lci=868; }		virtual int f869(int) { return lci=869; }		virtual int f870(int) { return lci=870; }		virtual int f871(int) { return lci=871; }
	virtual int f872(int) { return lci=872; }		virtual int f873(int) { return lci=873; }		virtual int f874(int) { return lci=874; }		virtual int f875(int) { return lci=875; }
	virtual int f876(int) { return lci=876; }		virtual int f877(int) { return lci=877; }		virtual int f878(int) { return lci=878; }		virtual int f879(int) { return lci=879; }
	virtual int f880(int) { return lci=880; }		virtual int f881(int) { return lci=881; }		virtual int f882(int) { return lci=882; }		virtual int f883(int) { return lci=883; }
	virtual int f884(int) { return lci=884; }		virtual int f885(int) { return lci=885; }		virtual int f886(int) { return lci=886; }		virtual int f887(int) { return lci=887; }
	virtual int f888(int) { return lci=888; }		virtual int f889(int) { return lci=889; }		virtual int f890(int) { return lci=890; }		virtual int f891(int) { return lci=891; }
	virtual int f892(int) { return lci=892; }		virtual int f893(int) { return lci=893; }		virtual int f894(int) { return lci=894; }		virtual int f895(int) { return lci=895; }
	virtual int f896(int) { return lci=896; }		virtual int f897(int) { return lci=897; }		virtual int f898(int) { return lci=898; }		virtual int f899(int) { return lci=899; }
	virtual int f900(int) { return lci=900; }		virtual int f901(int) { return lci=901; }		virtual int f902(int) { return lci=902; }		virtual int f903(int) { return lci=903; }
	virtual int f904(int) { return lci=904; }		virtual int f905(int) { return lci=905; }		virtual int f906(int) { return lci=906; }		virtual int f907(int) { return lci=907; }
	virtual int f908(int) { return lci=908; }		virtual int f909(int) { return lci=909; }		virtual int f910(int) { return lci=910; }		virtual int f911(int) { return lci=911; }
	virtual int f912(int) { return lci=912; }		virtual int f913(int) { return lci=913; }		virtual int f914(int) { return lci=914; }		virtual int f915(int) { return lci=915; }
	virtual int f916(int) { return lci=916; }		virtual int f917(int) { return lci=917; }		virtual int f918(int) { return lci=918; }		virtual int f919(int) { return lci=919; }
	virtual int f920(int) { return lci=920; }		virtual int f921(int) { return lci=921; }		virtual int f922(int) { return lci=922; }		virtual int f923(int) { return lci=923; }
	virtual int f924(int) { return lci=924; }		virtual int f925(int) { return lci=925; }		virtual int f926(int) { return lci=926; }		virtual int f927(int) { return lci=927; }
	virtual int f928(int) { return lci=928; }		virtual int f929(int) { return lci=929; }		virtual int f930(int) { return lci=930; }		virtual int f931(int) { return lci=931; }
	virtual int f932(int) { return lci=932; }		virtual int f933(int) { return lci=933; }		virtual int f934(int) { return lci=934; }		virtual int f935(int) { return lci=935; }
	virtual int f936(int) { return lci=936; }		virtual int f937(int) { return lci=937; }		virtual int f938(int) { return lci=938; }		virtual int f939(int) { return lci=939; }
	virtual int f940(int) { return lci=940; }		virtual int f941(int) { return lci=941; }		virtual int f942(int) { return lci=942; }		virtual int f943(int) { return lci=943; }
	virtual int f944(int) { return lci=944; }		virtual int f945(int) { return lci=945; }		virtual int f946(int) { return lci=946; }		virtual int f947(int) { return lci=947; }
	virtual int f948(int) { return lci=948; }		virtual int f949(int) { return lci=949; }		virtual int f950(int) { return lci=950; }		virtual int f951(int) { return lci=951; }
	virtual int f952(int) { return lci=952; }		virtual int f953(int) { return lci=953; }		virtual int f954(int) { return lci=954; }		virtual int f955(int) { return lci=955; }
	virtual int f956(int) { return lci=956; }		virtual int f957(int) { return lci=957; }		virtual int f958(int) { return lci=958; }		virtual int f959(int) { return lci=959; }
	virtual int f960(int) { return lci=960; }		virtual int f961(int) { return lci=961; }		virtual int f962(int) { return lci=962; }		virtual int f963(int) { return lci=963; }
	virtual int f964(int) { return lci=964; }		virtual int f965(int) { return lci=965; }		virtual int f966(int) { return lci=966; }		virtual int f967(int) { return lci=967; }
	virtual int f968(int) { return lci=968; }		virtual int f969(int) { return lci=969; }		virtual int f970(int) { return lci=970; }		virtual int f971(int) { return lci=971; }
	virtual int f972(int) { return lci=972; }		virtual int f973(int) { return lci=973; }		virtual int f974(int) { return lci=974; }		virtual int f975(int) { return lci=975; }
	virtual int f976(int) { return lci=976; }		virtual int f977(int) { return lci=977; }		virtual int f978(int) { return lci=978; }		virtual int f979(int) { return lci=979; }
	virtual int f980(int) { return lci=980; }		virtual int f981(int) { return lci=981; }		virtual int f982(int) { return lci=982; }		virtual int f983(int) { return lci=983; }
	virtual int f984(int) { return lci=984; }		virtual int f985(int) { return lci=985; }		virtual int f986(int) { return lci=986; }		virtual int f987(int) { return lci=987; }
	virtual int f988(int) { return lci=988; }		virtual int f989(int) { return lci=989; }		virtual int f990(int) { return lci=990; }		virtual int f991(int) { return lci=991; }
	virtual int f992(int) { return lci=992; }		virtual int f993(int) { return lci=993; }		virtual int f994(int) { return lci=994; }		virtual int f995(int) { return lci=995; }
	virtual int f996(int) { return lci=996; }		virtual int f997(int) { return lci=997; }		virtual int f998(int) { return lci=998; }		virtual int f999(int) { return lci=999; }
	virtual int f1000(int) { return lci=1000; }	virtual int f1001(int) { return lci=1001; }	virtual int f1002(int) { return lci=1002; }	virtual int f1003(int) { return lci=1003; }
	virtual int f1004(int) { return lci=1004; }	virtual int f1005(int) { return lci=1005; }	virtual int f1006(int) { return lci=1006; }	virtual int f1007(int) { return lci=1007; }
	virtual int f1008(int) { return lci=1008; }	virtual int f1009(int) { return lci=1009; }	virtual int f1010(int) { return lci=1010; }	virtual int f1011(int) { return lci=1011; }
	virtual int f1012(int) { return lci=1012; }	virtual int f1013(int) { return lci=1013; }	virtual int f1014(int) { return lci=1014; }	virtual int f1015(int) { return lci=1015; }
	virtual int f1016(int) { return lci=1016; }	virtual int f1017(int) { return lci=1017; }	virtual int f1018(int) { return lci=1018; }	virtual int f1019(int) { return lci=1019; }
	virtual int f1020(int) { return lci=1020; }	virtual int f1021(int) { return lci=1021; }	virtual int f1022(int) { return lci=1022; }	virtual int f1023(int) { return lci=1023; }
	virtual ~func_index() {}
};

#ifdef _MSC_VER
template <int s>
int virtual_function_index(unsigned char *func) {
	if (*func == 0xE9)
	{
		return virtual_function_index<0>(func + 5 + *(unsigned int *)(func+1));
	}
	else
	{
		switch(*(unsigned int *)func)
		{ // mov ecx, this; jump [eax + v/Ib/Iw]
		case 0x20ff018b: return 0;
		case 0x60ff018b: return *(unsigned char *)(func + 4) / 4;
		case 0xA0ff018b: return *(unsigned long *)(func + 4) / 4;
		default: return -1;
		}
	}
}
#endif

template <typename T>
std::pair<int, int> virtual_index(T t)
{
#if defined(__GNUG__)
	union {
		T t;
		struct
		{
			unsigned long value;
			unsigned long baseoffs;
		} u;
	} conv;
	conv.t = t;

	// simple implementation
	if (conv.u.value & 1)
		return std::pair<int, int>(conv.u.baseoffs / sizeof(void*), conv.u.value / sizeof(void *));
#elif defined(_MSC_VER)
	union {
		T t;
		struct
		{
			unsigned char *value;
			unsigned long baseoffs;
		} u;
	} conv;
	conv.t = t;

	int value = virtual_function_index<0>((unsigned char *)conv.u.value);
	if (value != -1)
		return std::pair<int, int>(conv.u.baseoffs/sizeof(void*), value);
#elif defined(__EDG__)
	union {
		T t;
		struct {
			short delta;
			short vindex;
			long vtordisp;
		} u;
	} conv;
	conv.t = t;

	if (conv.u.vindex != 0)
		return std::pair<int, int>((conv.u.delta + conv.u.vtordisp)/sizeof(void*), conv.u.vindex * 2 + 1);
#else
#error No virtual indexing found for this compiler! Please contact the maintainers of HippoMocks
#endif

	return std::pair<int, int>(-1, 0);
}

template <typename T, typename U>
T getNonvirtualMemberFunctionAddress(U u)
{
#ifdef __EDG__
	// Edison Design Group C++ frontend (Comeau, Portland Group, Greenhills, etc)
	union {
	  struct {
			short delta;
			short vindex;
			T t;
	  } mfp_structure;
	  U u;
	} conv;
#else
	// Visual Studio, GCC, others
	union {
	  struct {
			T t;
		} mfp_structure;
		U u;
	} conv;
#endif
	conv.u = u;
	return conv.mfp_structure.t;
}

class TypeDestructable {
public:
	virtual ~TypeDestructable() {}
};

template <typename A>
class MemberWrap : public TypeDestructable {
private:
	A *member;
public:
	MemberWrap(A *member)
		: member(member)
	{
		new (member) A();
	}
	~MemberWrap()
	{
		member->~A();
	}
};

// mock types
template <class T>
class mock : public base_mock
{
	typedef void (*funcptr)();
	friend class MockRepository;
	unsigned char remaining[sizeof(T)];
	void NotImplemented() {
		mock<T> *realMock = getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		throw ::NotImplementedException(repo);
	}
protected:
	std::map<int, void (**)()> funcTables;
	void (*notimplementedfuncs[VIRT_FUNC_LIMIT])();
public:
	bool isZombie;
	std::list<TypeDestructable *> members;
	MockRepository *repo;
	std::map<std::pair<int, int>, int> funcMap;
	mock(MockRepository *repo)
		: isZombie(false)
		, repo(repo)
	{
		for (int i = 0; i < VIRT_FUNC_LIMIT; i++)
		{
			notimplementedfuncs[i] = getNonvirtualMemberFunctionAddress<void (*)()>(&mock<T>::NotImplemented);
		}
		funcptr *funcTable = new funcptr[VIRT_FUNC_LIMIT+2];
		memcpy(funcTable, notimplementedfuncs, sizeof(funcptr) * VIRT_FUNC_LIMIT);
		((void **)funcTable)[VIRT_FUNC_LIMIT] = this;
		((void **)funcTable)[VIRT_FUNC_LIMIT+1] = *(void **)this;
		funcTables[0] = funcTable;
		*(void **)this = funcTable;
		for (unsigned int i = 1; i < sizeof(remaining) / sizeof(funcptr); i++)
		{
			((void **)this)[i] = (void *)notimplementedfuncs;
		}
	}
	~mock()
	{
		for (std::list<TypeDestructable *>::iterator i = members.begin(); i != members.end(); ++i)
		{
			delete *i;
		}
		for (std::map<int, void (**)()>::iterator i = funcTables.begin(); i != funcTables.end(); ++i)
		{
			delete [] i->second;
		}
	}
	void mock_reset()
	{
		MockRepository *repo = this->repo;
		// ugly but simple
		this->~mock<T>();
		new (this) mock<T>(repo);
	}
	mock<T> *getRealThis()
	{
		void ***base = (void ***)this;
		return (mock<T> *)((*base)[VIRT_FUNC_LIMIT]);
	}
	std::pair<int, int> translateX(int x)
	{
		for (std::map<std::pair<int, int>, int>::iterator i = funcMap.begin(); i != funcMap.end(); ++i)
		{
			if (i->second == x+1) return i->first;
		}
		return std::pair<int, int>(-1, 0);
	}
	template <int X>
	void mockedDestructor(int);
};

//Type-safe exception wrapping
class ExceptionHolder
{
public:
	virtual ~ExceptionHolder() {}
	virtual void rethrow() = 0;
};

template <class T>
class ExceptionWrapper : public ExceptionHolder {
	T exception;
public:
	ExceptionWrapper(T exception) : exception(exception) {}
	void rethrow() { throw exception; }
};

// Do() function wrapping
class VirtualDestructable { public: virtual ~VirtualDestructable() {} };

template <typename Y>
class TupleInvocable : public VirtualDestructable
{
public:
	virtual Y operator()(const base_tuple &tupl) = 0;
};

template <typename Y,
		  typename A = NullType, typename B = NullType, typename C = NullType, typename D = NullType,
		  typename E = NullType, typename F = NullType, typename G = NullType, typename H = NullType,
		  typename I = NullType, typename J = NullType, typename K = NullType, typename L = NullType,
		  typename M = NullType, typename N = NullType, typename O = NullType, typename P = NullType>
class Invocable : public TupleInvocable<Y>
{
public:
	virtual Y operator()(A a = A(), B b = B(), C c = C(), D d = D(), E e = E(), F f = F(), G g = G(), H h = H(), I i = I(), J j = J(), K k = K(), L l = L(), M m = M(), N n = N(), O o = O(), P p = P()) = 0;
	virtual Y operator()(const base_tuple &tupl) {
		const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &rTupl = reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl);
		return (*this)(rTupl.a, rTupl.b, rTupl.c, rTupl.d, rTupl.e, rTupl.f, rTupl.g, rTupl.h,
			rTupl.i, rTupl.j, rTupl.k, rTupl.l, rTupl.m, rTupl.n, rTupl.o, rTupl.p);
	}
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O, typename P>
class DoWrapper : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k,l,m,n);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k,l,m);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L,M>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k,l);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K,L>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J,K> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j,k);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J,K>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H,I,J> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i,j);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I,J>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType>  : public Invocable<Y,A,B,C,D,E,F,G,H,I>{
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, I i, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h,i);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H,I>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
class DoWrapper<T,Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G,H> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, H h, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g,h);
	}
	using Invocable<Y,A,B,C,D,E,F,G,H>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
class DoWrapper<T,Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F,G> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, G g, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f,g);
	}
	using Invocable<Y,A,B,C,D,E,F,G>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F>
class DoWrapper<T,Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E,F> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, F f, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e,f);
	}
	using Invocable<Y,A,B,C,D,E,F>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E>
class DoWrapper<T,Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D,E> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, E e, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d,e);
	}
	using Invocable<Y,A,B,C,D,E>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C, typename D>
class DoWrapper<T,Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C,D> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, D d, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c,d);
	}
	using Invocable<Y,A,B,C,D>::operator();
};
template <typename T, typename Y,
		  typename A, typename B, typename C>
class DoWrapper<T,Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B,C> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, C c, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b,c);
	}
	using Invocable<Y,A,B,C>::operator();
};
template <typename T, typename Y, typename A, typename B>
class DoWrapper<T,Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A,B> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, B b, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a,b);
	}
	using Invocable<Y,A,B>::operator();
};
template <typename T, typename Y, typename A>
class DoWrapper<T,Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y,A> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(A a, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t(a);
	}
	using Invocable<Y,A>::operator();
};
template <typename T, typename Y>
class DoWrapper<T,Y,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Invocable<Y> {
	T &t;
public:
	DoWrapper(T &t) : t(t) {}
	virtual Y operator()(NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType, NullType)
	{
		return t();
	}
	using Invocable<Y>::operator();
};

class ReturnValueHolder {
public:
	virtual ~ReturnValueHolder() {}
};

template <class T>
class ReturnValueWrapper : public ReturnValueHolder {
public:
	typename no_cref<T>::type rv;
	ReturnValueWrapper(T rv) : rv(rv) {}
};

//Call wrapping
class Call {
public:
	virtual bool matchesArgs(const base_tuple &tuple) = 0;
	virtual void assignArgs(base_tuple &tuple) = 0;
	ReturnValueHolder *retVal;
	ExceptionHolder *eHolder;
	base_mock *mock;
	VirtualDestructable *functor;
	VirtualDestructable *matchFunctor;
	std::pair<int, int> funcIndex;
	std::list<Call *> previousCalls;
	RegistrationType expectation;
	bool satisfied;
	int lineno;
	const char *funcName;
	const char *fileName;
protected:
	Call(RegistrationType expectation, base_mock *mock, const std::pair<int, int> &funcIndex, int X, const char *funcName, const char *fileName)
		: retVal(0),
		eHolder(0),
		mock(mock),
		functor(0),
		matchFunctor(0),
		funcIndex(funcIndex),
		expectation(expectation),
		satisfied(false),
		lineno(X),
		funcName(funcName),
		fileName(fileName)
	{
	}
public:
	virtual const base_tuple *getArgs() const = 0;
	virtual ~Call()
	{
		delete eHolder;
		delete functor;
		delete matchFunctor;
		delete retVal;
	}
};

std::ostream &operator<<(std::ostream &os, const Call &call);

template <typename Y,
		  typename A = NullType, typename B = NullType, typename C = NullType, typename D = NullType,
		  typename E = NullType, typename F = NullType, typename G = NullType, typename H = NullType,
		  typename I = NullType, typename J = NullType, typename K = NullType, typename L = NullType,
		  typename M = NullType, typename N = NullType, typename O = NullType, typename P = NullType>
class TCall : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN, typename CO, typename CP>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n, const CO & o, const CP & p) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,const CO &,const CP &>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p);
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O, typename P>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN, typename CO, typename CP>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n, const CO & o, const CP & p) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,const CO &,const CP &>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p);
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN, typename CO>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n, const CO & o) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,const CO &,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN, typename CO>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n, const CO & o) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,const CO &,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM, typename CN>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m, const CN & n) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,const CN &,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,N,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL,
			  typename CM>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l, const CM & m) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,const CM &,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,m,NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,M,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK, typename CL>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k, const CL & l) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,const CL &,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,l,NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,L,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
class TCall<void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ, typename CK>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j, const CK & k) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,const CK &,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,k,NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,K,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
class TCall<Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
class TCall<void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI, typename CJ>
	TCall<void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i, const CJ & j) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,const CJ &,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,j,NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,J,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
class TCall<Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI>
	TCall<Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
class TCall<void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH,
			  typename CI>
	TCall<void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h, const CI & i) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,const CI &,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,i,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,I,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
class TCall<Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH>
	TCall<Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
class TCall<void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG, typename CH>
	TCall<void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g, const CH & h) {
		args = new copy_tuple<A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,const CH &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,h,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,H,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
class TCall<Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG>
	TCall<Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g) {
		args = new copy_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
class TCall<void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF, typename CG>
	TCall<void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f, const CG & g) {
		args = new copy_tuple<A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,const CG &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,g,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,G,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F>
class TCall<Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF>
	TCall<Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f) {
		args = new copy_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E, typename F>
class TCall<void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE, typename CF>
	TCall<void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e, const CF & f) {
		args = new copy_tuple<A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,const CF &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,f,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,F,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E>
class TCall<Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE>
	TCall<Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e) {
		args = new copy_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D,
		  typename E>
class TCall<void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD,
			  typename CE>
	TCall<void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d, const CE & e) {
		args = new copy_tuple<A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,const CE &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,e,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,E,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C, typename D>
class TCall<Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD>
	TCall<Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d) {
		args = new copy_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C, typename D>
class TCall<void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC, typename CD>
	TCall<void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c, const CD & d) {
		args = new copy_tuple<A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,const CD &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,d,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,D,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B, typename C>
class TCall<Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC>
	TCall<Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c) {
		args = new copy_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B, typename C>
class TCall<void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB, typename CC>
	TCall<void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b, const CC & c) {
		args = new copy_tuple<A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,const CC &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,c,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,C,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A, typename B>
class TCall<Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB>
	TCall<Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b) {
		args = new copy_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A, typename B>
class TCall<void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA, typename CB>
	TCall<void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a, const CB & b) {
		args = new copy_tuple<A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,const CB &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,b,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,B,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y,
		  typename A>
class TCall<Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA>
	TCall<Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA & a) {
		args = new copy_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<Y,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <typename A>
class TCall<void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName), args(0) {}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &tupl) {
		return (!args && !matchFunctor) ||
			(args && (*args == reinterpret_cast<const ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl))) ||
			(matchFunctor && (*(TupleInvocable<bool> *)(matchFunctor))(tupl));
	}
	void assignArgs(base_tuple &tupl) {
		if(args) {
			args->assign_to(static_cast<ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
			args->assign_from(static_cast<ref_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &>(tupl));
		}
	}
	template <typename CA>
	TCall<void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &With(const CA &a) {
		args = new copy_tuple<A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								const CA &,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(a,NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
		return *this;
	}
	TCall<void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename T>
	TCall<void,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Match(T &function) { matchFunctor = new DoWrapper<T,bool,A,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <typename Y>
class TCall<Y,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName) {
		args = new copy_tuple<NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
								NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>
								(NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
	}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &) { return true; }
	void assignArgs(base_tuple &) {}
	TCall<Y,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<Y,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,Y,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	Call &Return(Y obj) { retVal = new ReturnValueWrapper<Y>(obj); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};
template <>
class TCall<void,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> : public Call {
private:
	ref_comparable_assignable_tuple<NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> *args;
public:
		const base_tuple *getArgs() const { return args; }
	TCall(RegistrationType expectation, base_mock *mock, std::pair<int, int> funcIndex, int X, const char *funcName, const char *fileName) : Call(expectation, mock, funcIndex, X, funcName, fileName) {
		args = new copy_tuple<NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,
							NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>
							(NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType(),NullType());
	}
	~TCall() { delete args; }
	bool matchesArgs(const base_tuple &) { return true; }
	void assignArgs(base_tuple &) { }
	TCall<void,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &After(Call &call) {
		previousCalls.push_back(&call);
		return *this;
	}
	template <typename T>
	TCall<void,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType> &Do(T &function) { functor = new DoWrapper<T,void,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType,NullType>(function); return *this; }
	template <typename Ex>
	Call &Throw(Ex exception) { eHolder = new ExceptionWrapper<Ex>(exception); return *this; }
};

template <int X>
class MockRepoInstanceHolder {
public:
	static MockRepository *instance;
};

template <int X>
MockRepository *MockRepoInstanceHolder<X>::instance;

class MockRepository {
private:
	friend inline std::ostream &operator<<(std::ostream &os, const MockRepository &repo);
	std::list<base_mock *> mocks;
  std::map<void (*)(), int> staticFuncMap;
  std::list<Replace *> staticReplaces;
	std::list<Call *> neverCalls;
	std::list<Call *> expectations;
	std::list<Call *> optionals;
public:
	bool autoExpect;
#ifdef _MSC_VER
#define OnCallFunc(func) RegisterExpect_<__COUNTER__, Any>(&func, #func, __FILE__, __LINE__)
#define ExpectCallFunc(func) RegisterExpect_<__COUNTER__, Once>(&func, #func, __FILE__, __LINE__)
#define NeverCallFunc(func) RegisterExpect_<__COUNTER__, Never>(&func, #func, __FILE__, __LINE__)
#define OnCall(obj, func) RegisterExpect_<__COUNTER__, Any>(obj, &func, #func, __FILE__, __LINE__)
#define ExpectCall(obj, func) RegisterExpect_<__COUNTER__, Once>(obj, &func, #func, __FILE__, __LINE__)
#define NeverCall(obj, func) RegisterExpect_<__COUNTER__, Never>(obj, &func, #func, __FILE__, __LINE__)
#define OnCallOverload(obj, func) RegisterExpect_<__COUNTER__, Any>(obj, func, #func, __FILE__, __LINE__)
#define ExpectCallOverload(obj, func) RegisterExpect_<__COUNTER__, Once>(obj, func, #func, __FILE__, __LINE__)
#define NeverCallOverload(obj, func) RegisterExpect_<__COUNTER__, Never>(obj, func, #func, __FILE__, __LINE__)
#define OnCallDestructor(obj) RegisterExpectDestructor<__COUNTER__, Any>(obj, __FILE__, __LINE__)
#define ExpectCallDestructor(obj) RegisterExpectDestructor<__COUNTER__, Once>(obj, __FILE__, __LINE__)
#define NeverCallDestructor(obj) RegisterExpectDestructor<__COUNTER__, Never>(obj, __FILE__, __LINE__)
#else
#define OnCallFunc(func) RegisterExpect_<__LINE__, Any>(&func, #func, __FILE__, __LINE__)
#define ExpectCallFunc(func) RegisterExpect_<__LINE__, Once>(&func, #func, __FILE__, __LINE__)
#define NeverCallFunc(func) RegisterExpect_<__LINE__, Never>(&func, #func, __FILE__, __LINE__)
#define OnCall(obj, func) RegisterExpect_<__LINE__, Any>(obj, &func, #func, __FILE__, __LINE__)
#define ExpectCall(obj, func) RegisterExpect_<__LINE__, Once>(obj, &func, #func, __FILE__, __LINE__)
#define NeverCall(obj, func) RegisterExpect_<__LINE__, Never>(obj, &func, #func, __FILE__, __LINE__)
#define OnCallOverload(obj, func) RegisterExpect_<__LINE__, Any>(obj, func, #func, __FILE__, __LINE__)
#define ExpectCallOverload(obj, func) RegisterExpect_<__LINE__, Once>(obj, func, #func, __FILE__, __LINE__)
#define NeverCallOverload(obj, func) RegisterExpect_<__LINE__, Never>(obj, func, #func, __FILE__, __LINE__)
#define OnCallDestructor(obj) RegisterExpectDestructor<__LINE__, Any>(obj, __FILE__, __LINE__)
#define ExpectCallDestructor(obj) RegisterExpectDestructor<__LINE__, Once>(obj, __FILE__, __LINE__)
#define NeverCallDestructor(obj) RegisterExpectDestructor<__LINE__, Never>(obj, __FILE__, __LINE__)
#endif
	template <typename A, class B, typename C>
	void Member(A *mck, C B::*member)
	{
		C A::*realMember = (C A::*)member;
		C *realRealMember = &(mck->*realMember);
		mock<A> *realMock = (mock<A> *)mck;
		realMock->members.push_back(new MemberWrap<C>(realRealMember));
	}
  template <int X, RegistrationType expect, typename Z2>
	TCall<void> &RegisterExpectDestructor(Z2 *mck, const char *fileName, unsigned long lineNo);

	template <int X, RegistrationType expect, typename Y>
	TCall<Y> &RegisterExpect_(Y (*func)(), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y, typename A>
	TCall<Y,A> &RegisterExpect_(Y (*func)(A), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Y (*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Y (*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Y (*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Y (*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Y (*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Y,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo);

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (Z::*func)(), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo);

	//GCC 3.x doesn't seem to understand overloading on const or non-const member function. Volatile appears to work though.
#if !defined(__GNUC__) || __GNUC__ > 3
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (Z::*func)() volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (Z::*func)() const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (Z::*func)() const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }
#endif

#ifdef _MSC_VER
	// COM only support - you can duplicate this for cdecl and fastcall if you want to, but those are not as common as COM.
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::* func)(), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo);
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo);

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)() volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)() const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) const volatile, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }

	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
	TCall<Y> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)() const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)())(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
	TCall<Y,A> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B>
	TCall<Y,A,B> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C>
	TCall<Y,A,B,C> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D>
	TCall<Y,A,B,C,D> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E>
	TCall<Y,A,B,C,D,E> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F>
	TCall<Y,A,B,C,D,E,F> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G>
	TCall<Y,A,B,C,D,E,F,G> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H>
	TCall<Y,A,B,C,D,E,F,G,H> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I>
	TCall<Y,A,B,C,D,E,F,G,H,I> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J>
	TCall<Y,A,B,C,D,E,F,G,H,I,J> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))(func), funcName, fileName, lineNo); }
	template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
			  typename A, typename B, typename C, typename D,
			  typename E, typename F, typename G, typename H,
			  typename I, typename J, typename K, typename L,
			  typename M, typename N, typename O, typename P>
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) const, const char *funcName, const char *fileName, unsigned long lineNo) { return RegisterExpect_<X,expect>(mck, (Y(__stdcall Z::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))(func), funcName, fileName, lineNo); }
#endif

	template <typename Z>
	void BasicRegisterExpect(mock<Z> *zMock, int baseOffset, int funcIndex, void (base_mock::*func)(), int X);
  int BasicStaticRegisterExpect(void (*func)(), void (*fp)(), int X)
  {
    if (staticFuncMap.find(func) == staticFuncMap.end())
    {
      staticFuncMap[func] = X;
      staticReplaces.push_back(new Replace(func, fp));
    }
    return staticFuncMap[func];
  }

	template <typename Z>
	Z DoExpectation(base_mock *mock, std::pair<int, int> funcno, const base_tuple &tuple);
	void DoVoidExpectation(base_mock *mock, std::pair<int, int> funcno, const base_tuple &tuple)
	{
		for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end(); ++i)
		{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno &&
				call->matchesArgs(tuple) &&
				!call->satisfied)
			{
				bool allSatisfy = true;
				for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
					callsBefore != call->previousCalls.end(); ++callsBefore)
				{
					if (!(*callsBefore)->satisfied)
					{
						allSatisfy = false;
					}
				}
				if (!allSatisfy) continue;

				call->satisfied = true;

				call->assignArgs(const_cast<base_tuple &>(tuple));

				if (call->eHolder)
					call->eHolder->rethrow();

				if (call->functor != NULL)
					(*(TupleInvocable<void> *)(call->functor))(tuple);

				return;
	    	}
		}
		for (std::list<Call *>::iterator i = neverCalls.begin(); i != neverCalls.end(); ++i)
		{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno &&
				call->matchesArgs(tuple))
			{
				bool allSatisfy = true;
				for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
					callsBefore != call->previousCalls.end(); ++callsBefore)
				{
					if (!(*callsBefore)->satisfied)
					{
						allSatisfy = false;
					}
				}
				if (!allSatisfy) continue;

				call->satisfied = true;

				throw ExpectationException(this, call->getArgs(), call->funcName);
			}
		}
		for (std::list<Call *>::iterator i = optionals.begin(); i != optionals.end(); ++i)
		{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno &&
				call->matchesArgs(tuple))
			{
				bool allSatisfy = true;
				for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
					callsBefore != call->previousCalls.end(); ++callsBefore)
				{
					if (!(*callsBefore)->satisfied)
					{
						allSatisfy = false;
					}
				}
				if (!allSatisfy) continue;

				call->satisfied = true;

				call->assignArgs(const_cast<base_tuple &>(tuple));

				if (call->eHolder)
					call->eHolder->rethrow();

				if (call->functor != NULL)
					(*(TupleInvocable<void> *)(call->functor))(tuple);

				return;
			}
		}
		const char *funcName = NULL;
		for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end() && !funcName; ++i)
	  	{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno)
			funcName = call->funcName;
		}
		for (std::list<Call *>::iterator i = optionals.begin(); i != optionals.end() && !funcName; ++i)
		{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno)
			funcName = call->funcName;
		}
		for (std::list<Call *>::iterator i = neverCalls.begin(); i != neverCalls.end() && !funcName; ++i)
		{
			Call *call = *i;
			if (call->mock == mock &&
				call->funcIndex == funcno)
				funcName = call->funcName;
		}
				throw ExpectationException(this, &tuple, funcName);
		}
		MockRepository()
			: autoExpect(true)
		{
			MockRepoInstanceHolder<0>::instance = this;
		}
		~MockRepository()
		{
			MockRepoInstanceHolder<0>::instance = 0;
		if (!std::uncaught_exception())
		{
			try
			{
				VerifyAll();
			}
			catch(...)
			{
				reset();
				for (std::list<base_mock *>::iterator i = mocks.begin(); i != mocks.end(); i++)
				{
					(*i)->destroy();
				}
    		for (std::list<Replace *>::iterator i = staticReplaces.begin(); i != staticReplaces.end(); i++)
    		{
    			delete *i;
    		}
				throw;
			}
		}
		reset();
		for (std::list<base_mock *>::iterator i = mocks.begin(); i != mocks.end(); i++)
		{
			(*i)->destroy();
		}
		for (std::list<Replace *>::iterator i = staticReplaces.begin(); i != staticReplaces.end(); i++)
		{
			delete *i;
		}
	}
	void reset()
	{
		for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end(); i++)
		{
			delete *i;
		}
		expectations.clear();
		for (std::list<Call *>::iterator i = neverCalls.begin(); i != neverCalls.end(); i++)
		{
			delete *i;
		}
		neverCalls.clear();
		for (std::list<Call *>::iterator i = optionals.begin(); i != optionals.end(); i++)
		{
			delete *i;
		}
		optionals.clear();
		for (std::list<base_mock *>::iterator i = mocks.begin(); i != mocks.end(); i++)
		{
			(*i)->reset();
		}
	}
	void VerifyAll()
	{
		for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end(); i++)
		{
			if (!(*i)->satisfied)
	    		throw CallMissingException(this);
		}
		}
	void VerifyPartial(base_mock *obj)
	{
		for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end(); i++)
		{
			if ((*i)->mock == (base_mock *)obj &&
				!(*i)->satisfied)
	    		throw CallMissingException(this);
		}
	}
	template <typename base>
	base *Mock();
};

// mock function providers
template <typename Z, typename Y>
class mockFuncs : public mock<Z> {
private:
	mockFuncs();
public:
	template <int X>
	Y expectation0()
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<>());
	}
	template <int X, typename A>
	Y expectation1(A a)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	Y expectation2(A a, B b)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	Y expectation3(A a, B b, C c)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	Y expectation4(A a, B b, C c, D d)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	Y expectation5(A a, B b, C c, D d, E e)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	Y expectation6(A a, B b, C c, D d, E e, F f)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	Y expectation7(A a, B b, C c, D d, E e, F f, G g)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	Y expectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	Y expectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	Y expectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	Y expectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	Y expectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	Y expectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	Y expectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	Y expectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	Y expectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		return repo->template DoExpectation<Y>(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}

	template <int X>
	static Y static_expectation0()
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<>());
	}
	template <int X, typename A>
	static Y static_expectation1(A a)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	static Y static_expectation2(A a, B b)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	static Y static_expectation3(A a, B b, C c)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	static Y static_expectation4(A a, B b, C c, D d)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	static Y static_expectation5(A a, B b, C c, D d, E e)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	static Y static_expectation6(A a, B b, C c, D d, E e, F f)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	static Y static_expectation7(A a, B b, C c, D d, E e, F f, G g)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	static Y static_expectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	static Y static_expectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	static Y static_expectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	static Y static_expectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	static Y static_expectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	static Y static_expectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	static Y static_expectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	static Y static_expectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	static Y static_expectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
		return MockRepoInstanceHolder<0>::instance->template DoExpectation<Y>(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}

#ifdef _MSC_VER
	template <int X>
	Y __stdcall stdcallexpectation0()
	{
        MockRepository *repo = mock<Z>::repo;
        return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<>());
	}
	template <int X, typename A>
	Y __stdcall stdcallexpectation1(A a)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	Y __stdcall stdcallexpectation2(A a, B b)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	Y __stdcall stdcallexpectation3(A a, B b, C c)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	Y __stdcall stdcallexpectation4(A a, B b, C c, D d)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	Y __stdcall stdcallexpectation5(A a, B b, C c, D d, E e)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	Y __stdcall stdcallexpectation6(A a, B b, C c, D d, E e, F f)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	Y __stdcall stdcallexpectation7(A a, B b, C c, D d, E e, F f, G g)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	Y __stdcall stdcallexpectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	Y __stdcall stdcallexpectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	Y __stdcall stdcallexpectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	Y __stdcall stdcallexpectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	Y __stdcall stdcallexpectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	Y __stdcall stdcallexpectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	Y __stdcall stdcallexpectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
        MockRepository *repo = mock<Z>::repo;
        return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	Y __stdcall stdcallexpectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	Y __stdcall stdcallexpectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
        MockRepository *repo = mock<Z>::repo;
		return repo->template DoExpectation<Y>(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}
#endif
};

template <typename Z>
class mockFuncs<Z, void> : public mock<Z> {
private:
		mockFuncs();
public:
	template <int X>
	void expectation0()
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<>());
	}
	template <int X, typename A>
	void expectation1(A a)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	void expectation2(A a, B b)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	void expectation3(A a, B b, C c)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	void expectation4(A a, B b, C c, D d)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	void expectation5(A a, B b, C c, D d, E e)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	void expectation6(A a, B b, C c, D d, E e, F f)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	void expectation7(A a, B b, C c, D d, E e, F f, G g)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	void expectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	void expectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	void expectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	void expectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	void expectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	void expectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	void expectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	void expectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	void expectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
		mock<Z> *realMock = mock<Z>::getRealThis();
		if (realMock->isZombie)
			throw ZombieMockException(realMock->repo);
		MockRepository *repo = realMock->repo;
		repo->DoVoidExpectation(realMock, realMock->translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}
	template <int X>
	static void static_expectation0()
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<>());
	}
	template <int X, typename A>
	static void static_expectation1(A a)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	static void static_expectation2(A a, B b)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	static void static_expectation3(A a, B b, C c)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	static void static_expectation4(A a, B b, C c, D d)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	static void static_expectation5(A a, B b, C c, D d, E e)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	static void static_expectation6(A a, B b, C c, D d, E e, F f)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	static void static_expectation7(A a, B b, C c, D d, E e, F f, G g)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	static void static_expectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	static void static_expectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	static void static_expectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	static void static_expectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	static void static_expectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	static void static_expectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	static void static_expectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	static void static_expectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	static void static_expectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
		MockRepoInstanceHolder<0>::instance->DoVoidExpectation(NULL, std::pair<int, int>(0, X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}

#ifdef _MSC_VER
	template <int X>
	void __stdcall stdcallexpectation0()
	{
        MockRepository *repo = mock<Z>::repo;
        repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<>());
	}
	template <int X, typename A>
	void __stdcall stdcallexpectation1(A a)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A>(a));
	}
	template <int X, typename A, typename B>
	void __stdcall stdcallexpectation2(A a, B b)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B>(a,b));
	}
	template <int X, typename A, typename B, typename C>
	void __stdcall stdcallexpectation3(A a, B b, C c)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C>(a,b,c));
	}
	template <int X, typename A, typename B, typename C, typename D>
	void __stdcall stdcallexpectation4(A a, B b, C c, D d)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D>(a,b,c,d));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E>
	void __stdcall stdcallexpectation5(A a, B b, C c, D d, E e)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E>(a,b,c,d,e));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F>
	void __stdcall stdcallexpectation6(A a, B b, C c, D d, E e, F f)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F>(a,b,c,d,e,f));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
	void __stdcall stdcallexpectation7(A a, B b, C c, D d, E e, F f, G g)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G>(a,b,c,d,e,f,g));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
	void __stdcall stdcallexpectation8(A a, B b, C c, D d, E e, F f, G g, H h)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H>(a,b,c,d,e,f,g,h));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
	void __stdcall stdcallexpectation9(A a, B b, C c, D d, E e, F f, G g, H h, I i)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I>(a,b,c,d,e,f,g,h,i));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
	void __stdcall stdcallexpectation10(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J>(a,b,c,d,e,f,g,h,i,j));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
	void __stdcall stdcallexpectation11(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K>(a,b,c,d,e,f,g,h,i,j,k));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
	void __stdcall stdcallexpectation12(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L>(a,b,c,d,e,f,g,h,i,j,k,l));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
	void __stdcall stdcallexpectation13(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M>(a,b,c,d,e,f,g,h,i,j,k,l,m));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
	void __stdcall stdcallexpectation14(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n)
	{
        MockRepository *repo = mock<Z>::repo;
        repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N>(a,b,c,d,e,f,g,h,i,j,k,l,m,n));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O>
	void __stdcall stdcallexpectation15(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o));
	}
	template <int X, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N, typename O, typename P>
	void __stdcall stdcallexpectation16(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j, K k, L l, M m, N n, O o, P p)
	{
        MockRepository *repo = mock<Z>::repo;
		repo->DoVoidExpectation(this, mock<Z>::translateX(X), ref_tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p));
	}
#endif
};

template <typename T>
template <int X>
void mock<T>::mockedDestructor(int)
{
	repo->DoVoidExpectation(this, translateX(X), ref_tuple<>());
	repo->VerifyPartial(this);
	isZombie = true;
}

template <typename Z>
void MockRepository::BasicRegisterExpect(mock<Z> *zMock, int baseOffset, int funcIndex, void (base_mock::*func)(), int X)
{
	if (funcIndex > VIRT_FUNC_LIMIT) throw NotImplementedException(this);
	if ((unsigned int)baseOffset * sizeof(void*) + sizeof(void*)-1 > sizeof(Z)) throw NotImplementedException(this);
	if (zMock->funcMap.find(std::make_pair(baseOffset, funcIndex)) == zMock->funcMap.end())
	{
		if (zMock->funcTables.find(baseOffset) == zMock->funcTables.end())
		{
			typedef void (*funcptr)();
			funcptr *funcTable = new funcptr[VIRT_FUNC_LIMIT+1];
			memcpy(funcTable, zMock->notimplementedfuncs, sizeof(funcptr) * VIRT_FUNC_LIMIT);
			((void **)funcTable)[VIRT_FUNC_LIMIT] = zMock;
			zMock->funcTables[baseOffset] = funcTable;
			((void **)zMock)[baseOffset] = funcTable;
		}
		zMock->funcMap[std::make_pair(baseOffset, funcIndex)] = X+1;
		zMock->funcTables[baseOffset][funcIndex] = getNonvirtualMemberFunctionAddress<void (*)()>(func);
	}
}

template <int X, RegistrationType expect, typename Z2>
TCall<void> &MockRepository::RegisterExpectDestructor(Z2 *mck, const char *fileName, unsigned long lineNo)
{
	func_index idx;
	((Z2 *)&idx)->~Z2();
	int funcIndex = idx.lci * FUNCTION_STRIDE + FUNCTION_BASE;
	void (mock<Z2>::*member)(int);
	member = &mock<Z2>::template mockedDestructor<X>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						0, funcIndex,
						reinterpret_cast<void (base_mock::*)()>(member), X);
#ifdef EXTRA_DESTRUCTOR
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						0, funcIndex+1,
						reinterpret_cast<void (base_mock::*)()>(member), X);
#endif
	TCall<void> *call = new TCall<void>(Once, reinterpret_cast<base_mock *>(mck), std::pair<int, int>(0, funcIndex), lineNo, "destructor", fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
    break;
  }
	return *call;
}

#ifdef _MSC_VER
// Support for COM, see declarations
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
TCall<Y> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)();
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation0<X>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y> *call = new TCall<Y>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
TCall<Y,A> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation1<X,A>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A> *call = new TCall<Y,A>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B>
TCall<Y,A,B> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation2<X,A,B>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B> *call = new TCall<Y,A,B>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C>
TCall<Y,A,B,C> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation3<X,A,B,C>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C> *call = new TCall<Y,A,B,C>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D>
TCall<Y,A,B,C,D> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation4<X,A,B,C,D>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D> *call = new TCall<Y,A,B,C,D>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E>
TCall<Y,A,B,C,D,E> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation5<X,A,B,C,D,E>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E> *call = new TCall<Y,A,B,C,D,E>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F>
TCall<Y,A,B,C,D,E,F> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation6<X,A,B,C,D,E,F>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F> *call = new TCall<Y,A,B,C,D,E,F>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
TCall<Y,A,B,C,D,E,F,G> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation7<X,A,B,C,D,E,F,G>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G> *call = new TCall<Y,A,B,C,D,E,F,G>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
TCall<Y,A,B,C,D,E,F,G,H> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation8<X,A,B,C,D,E,F,G,H>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H> *call = new TCall<Y,A,B,C,D,E,F,G,H>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
TCall<Y,A,B,C,D,E,F,G,H,I> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation9<X,A,B,C,D,E,F,G,H,I>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I> *call = new TCall<Y,A,B,C,D,E,F,G,H,I>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
TCall<Y,A,B,C,D,E,F,G,H,I,J> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation10<X,A,B,C,D,E,F,G,H,I,J>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation11<X,A,B,C,D,E,F,G,H,I,J,K>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation12<X,A,B,C,D,E,F,G,H,I,J,K,L>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation13<X,A,B,C,D,E,F,G,H,I,J,K,L,M>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation14<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation15<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}

template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O, typename P>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &MockRepository::RegisterExpect_(Z2 *mck, Y (__stdcall Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo)
{
	int funcIndex = getFunctionIndex(func);
	Y (__stdcall mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P);
	mfp = &mockFuncs<Z2, Y>::template stdcallexpectation16<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case DontCare: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
#endif

template <int X, RegistrationType expect, typename Y>
TCall<Y> &MockRepository::RegisterExpect_(Y (*func)(), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)();
  fp = &mockFuncs<char, Y>::template static_expectation0<X>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y> *call = new TCall<Y>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y, typename A>
TCall<Y,A> &MockRepository::RegisterExpect_(Y (*func)(A), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A);
  fp = &mockFuncs<char, Y>::template static_expectation1<X,A>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A> *call = new TCall<Y,A>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B>
TCall<Y,A,B> &MockRepository::RegisterExpect_(Y (*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B);
  fp = &mockFuncs<char, Y>::template static_expectation2<X,A,B>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B> *call = new TCall<Y,A,B>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C>
TCall<Y,A,B,C> &MockRepository::RegisterExpect_(Y (*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C);
  fp = &mockFuncs<char, Y>::template static_expectation3<X,A,B,C>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C> *call = new TCall<Y,A,B,C>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D>
TCall<Y,A,B,C,D> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D);
  fp = &mockFuncs<char, Y>::template static_expectation4<X,A,B,C,D>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D> *call = new TCall<Y,A,B,C,D>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E>
TCall<Y,A,B,C,D,E> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E);
  fp = &mockFuncs<char, Y>::template static_expectation5<X,A,B,C,D,E>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E> *call = new TCall<Y,A,B,C,D,E>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F>
TCall<Y,A,B,C,D,E,F> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F);
  fp = &mockFuncs<char, Y>::template static_expectation6<X,A,B,C,D,E,F>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F> *call = new TCall<Y,A,B,C,D,E,F>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
TCall<Y,A,B,C,D,E,F,G> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G);
  fp = &mockFuncs<char, Y>::template static_expectation7<X,A,B,C,D,E,F,G>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G> *call = new TCall<Y,A,B,C,D,E,F,G>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
TCall<Y,A,B,C,D,E,F,G,H> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H);
  fp = &mockFuncs<char, Y>::template static_expectation8<X,A,B,C,D,E,F,G,H>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H> *call = new TCall<Y,A,B,C,D,E,F,G,H>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
TCall<Y,A,B,C,D,E,F,G,H,I> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I);
  fp = &mockFuncs<char, Y>::template static_expectation9<X,A,B,C,D,E,F,G,H,I>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I> *call = new TCall<Y,A,B,C,D,E,F,G,H,I>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
TCall<Y,A,B,C,D,E,F,G,H,I,J> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J);
  fp = &mockFuncs<char, Y>::template static_expectation10<X,A,B,C,D,E,F,G,H,I,J>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K);
  fp = &mockFuncs<char, Y>::template static_expectation11<X,A,B,C,D,E,F,G,H,I,J,K>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K,L);
  fp = &mockFuncs<char, Y>::template static_expectation12<X,A,B,C,D,E,F,G,H,I,J,K,L>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K,L,M);
  fp = &mockFuncs<char, Y>::template static_expectation13<X,A,B,C,D,E,F,G,H,I,J,K,L,M>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N);
  fp = &mockFuncs<char, Y>::template static_expectation14<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}
template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O);
  fp = &mockFuncs<char, Y>::template static_expectation15<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}

template <int X, RegistrationType expect, typename Y,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O, typename P>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &MockRepository::RegisterExpect_(Y (*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo)
{
  Y (*fp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P);
  fp = &mockFuncs<char, Y>::template static_expectation16<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>;
  int index = BasicStaticRegisterExpect(reinterpret_cast<void (*)()>(func), reinterpret_cast<void (*)()>(fp),X);
  TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(expect, NULL, std::pair<int, int>(0, index), lineNo, funcName, fileName);
  switch(expect)
  {
  case Never: neverCalls.push_back(call); break;
  case Any: optionals.push_back(call); break;
  case Once:
    if (autoExpect && expectations.size() > 0)
    {
      call->previousCalls.push_back(expectations.back());
    }
    expectations.push_back(call);
      break;
  }
  return *call;
}

template <int X, RegistrationType expect, typename Z2, typename Y, typename Z>
TCall<Y> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y (Z2::*)())func);
	Y (mockFuncs<Z2, Y>::*mfp)();
	mfp = &mockFuncs<Z2, Y>::template expectation0<X>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y> *call = new TCall<Y>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z, typename A>
TCall<Y,A> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A);
	mfp = &mockFuncs<Z2, Y>::template expectation1<X,A>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A> *call = new TCall<Y,A>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B>
TCall<Y,A,B> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B);
	mfp = &mockFuncs<Z2, Y>::template expectation2<X,A,B>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B> *call = new TCall<Y,A,B>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C>
TCall<Y,A,B,C> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C);
	mfp = &mockFuncs<Z2, Y>::template expectation3<X,A,B,C>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C> *call = new TCall<Y,A,B,C>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D>
TCall<Y,A,B,C,D> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D);
	mfp = &mockFuncs<Z2, Y>::template expectation4<X,A,B,C,D>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D> *call = new TCall<Y,A,B,C,D>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E>
TCall<Y,A,B,C,D,E> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E);
	mfp = &mockFuncs<Z2, Y>::template expectation5<X,A,B,C,D,E>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E> *call = new TCall<Y,A,B,C,D,E>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F>
TCall<Y,A,B,C,D,E,F> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F);
	mfp = &mockFuncs<Z2, Y>::template expectation6<X,A,B,C,D,E,F>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F> *call = new TCall<Y,A,B,C,D,E,F>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G>
TCall<Y,A,B,C,D,E,F,G> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G);
	mfp = &mockFuncs<Z2, Y>::template expectation7<X,A,B,C,D,E,F,G>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G> *call = new TCall<Y,A,B,C,D,E,F,G>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H>
TCall<Y,A,B,C,D,E,F,G,H> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H);
	mfp = &mockFuncs<Z2, Y>::template expectation8<X,A,B,C,D,E,F,G,H>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H> *call = new TCall<Y,A,B,C,D,E,F,G,H>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I>
TCall<Y,A,B,C,D,E,F,G,H,I> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I);
	mfp = &mockFuncs<Z2, Y>::template expectation9<X,A,B,C,D,E,F,G,H,I>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I> *call = new TCall<Y,A,B,C,D,E,F,G,H,I>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J>
TCall<Y,A,B,C,D,E,F,G,H,I,J> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J);
	mfp = &mockFuncs<Z2, Y>::template expectation10<X,A,B,C,D,E,F,G,H,I,J>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
		break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K);
	mfp = &mockFuncs<Z2, Y>::template expectation11<X,A,B,C,D,E,F,G,H,I,J,K>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K,L))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L);
	mfp = &mockFuncs<Z2, Y>::template expectation12<X,A,B,C,D,E,F,G,H,I,J,K,L>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K,L,M))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M);
	mfp = &mockFuncs<Z2, Y>::template expectation13<X,A,B,C,D,E,F,G,H,I,J,K,L,M>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N);
	mfp = &mockFuncs<Z2, Y>::template expectation14<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}
template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O);
	mfp = &mockFuncs<Z2, Y>::template expectation15<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}

template <int X, RegistrationType expect, typename Z2, typename Y, typename Z,
		  typename A, typename B, typename C, typename D,
		  typename E, typename F, typename G, typename H,
		  typename I, typename J, typename K, typename L,
		  typename M, typename N, typename O, typename P>
TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> &MockRepository::RegisterExpect_(Z2 *mck, Y (Z::*func)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P), const char *funcName, const char *fileName, unsigned long lineNo)
{
	std::pair<int, int> funcIndex = virtual_index((Y(Z2::*)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P))func);
	Y (mockFuncs<Z2, Y>::*mfp)(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P);
	mfp = &mockFuncs<Z2, Y>::template expectation16<X,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>;
	BasicRegisterExpect(reinterpret_cast<mock<Z2> *>(mck),
						funcIndex.first, funcIndex.second,
						reinterpret_cast<void (base_mock::*)()>(mfp),X);
	TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P> *call = new TCall<Y,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P>(expect, reinterpret_cast<base_mock *>(mck), funcIndex, lineNo, funcName, fileName);
	switch(expect)
	{
	case Never: neverCalls.push_back(call); break;
	case Any: optionals.push_back(call); break;
	case Once:
		if (autoExpect && expectations.size() > 0)
		{
			call->previousCalls.push_back(expectations.back());
		}
		expectations.push_back(call);
	    break;
	}
	return *call;
}

template <typename Z>
Z MockRepository::DoExpectation(base_mock *mock, std::pair<int, int> funcno, const base_tuple &tuple)
{
	for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end(); ++i)
	{
		Call *call = *i;
		if (call->mock == mock &&
			call->funcIndex == funcno &&
			call->matchesArgs(tuple) &&
			!call->satisfied)
		{
			bool allSatisfy = true;
			for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
				callsBefore != call->previousCalls.end(); ++callsBefore)
			{
				if (!(*callsBefore)->satisfied)
				{
					allSatisfy = false;
				}
			}
			if (!allSatisfy) continue;

			call->satisfied = true;

			call->assignArgs(const_cast<base_tuple &>(tuple));

			if (call->eHolder)
				call->eHolder->rethrow();

			if (call->functor != NULL)
			{
				if(call->retVal == NULL)
				{
					return (*(TupleInvocable<Z> *)(call->functor))(tuple);
				}
				else
				{
					(*(TupleInvocable<Z> *)(call->functor))(tuple);
				}
			}

			if (call->retVal)
				return ((ReturnValueWrapper<Z> *)call->retVal)->rv;

			throw NoResultSetUpException(this, call->getArgs(), call->funcName);
		}
	}
	for (std::list<Call *>::iterator i = neverCalls.begin(); i != neverCalls.end(); ++i)
	{
		Call *call = *i;
		if (call->mock == mock &&
			call->funcIndex == funcno &&
			call->matchesArgs(tuple))
		{
			bool allSatisfy = true;
			for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
				callsBefore != call->previousCalls.end(); ++callsBefore)
			{
				if (!(*callsBefore)->satisfied)
				{
					allSatisfy = false;
				}
			}
			if (!allSatisfy) continue;

			call->satisfied = true;

			throw ExpectationException(this, call->getArgs(), call->funcName);
		}
	}
	for (std::list<Call *>::iterator i = optionals.begin(); i != optionals.end(); ++i)
	{
		Call *call = *i;
		if (call->mock == mock &&
			call->funcIndex == funcno &&
			call->matchesArgs(tuple))
		{
			bool allSatisfy = true;
			for (std::list<Call *>::iterator callsBefore = call->previousCalls.begin();
				callsBefore != call->previousCalls.end(); ++callsBefore)
			{
				if (!(*callsBefore)->satisfied)
				{
					allSatisfy = false;
				}
			}
			if (!allSatisfy) continue;

			call->satisfied = true;

			call->assignArgs(const_cast<base_tuple &>(tuple));

			if (call->eHolder)
				call->eHolder->rethrow();

			if (call->functor != NULL)
			{
				if(call->retVal == NULL)
				{
					return (*(TupleInvocable<Z> *)(call->functor))(tuple);
				}
				else
				{
					(*(TupleInvocable<Z> *)(call->functor))(tuple);
				}
			}

			if (call->retVal)
				return ((ReturnValueWrapper<Z> *)call->retVal)->rv;

			throw NoResultSetUpException(this, call->getArgs(), call->funcName);
		}
	}
	const char *funcName = NULL;
	for (std::list<Call *>::iterator i = expectations.begin(); i != expectations.end() && !funcName; ++i)
	{
		Call *call = *i;
		if (call->mock == mock &&
			call->funcIndex == funcno)
			funcName = call->funcName;
	}
	for (std::list<Call *>::iterator i = neverCalls.begin(); i != neverCalls.end() && !funcName; ++i)
		{
	    Call *call = *i;
	    if (call->mock == mock &&
		    call->funcIndex == funcno)
				funcName = call->funcName;
		}
	for (std::list<Call *>::iterator i = optionals.begin(); i != optionals.end() && !funcName; ++i)
		{
	    Call *call = *i;
	    if (call->mock == mock &&
		    call->funcIndex == funcno)
				funcName = call->funcName;
		}
	throw ExpectationException(this, &tuple, funcName);
}
template <typename base>
base *MockRepository::Mock() {
	mock<base> *m = new mock<base>(this);
		mocks.push_back(m);
	return reinterpret_cast<base *>(m);
}
inline std::ostream &operator<<(std::ostream &os, const Call &call)
{
	os << call.fileName << "(" << call.lineno << ") ";
	if (call.expectation == Once)
		os << "Expectation for ";
	else
		os << "Result set for ";

	os << call.funcName;

		if (call.getArgs())
				call.getArgs()->printTo(os);
		else
				os << "(...)";

		os << " on the mock at 0x" << call.mock << " was ";

	if (!call.satisfied)
		os << "not ";

	if (call.expectation == Once)
		os << "satisfied." << std::endl;
	else
		os << "used." << std::endl;

	return os;
}

inline std::ostream &operator<<(std::ostream &os, const MockRepository &repo)
{
	if (repo.expectations.size())
	{
		os << "Expections set:" << std::endl;
		for (std::list<Call *>::const_iterator exp = repo.expectations.begin(); exp != repo.expectations.end(); ++exp)
			os << **exp;
		os << std::endl;
	}

	if (repo.neverCalls.size())
	{
		os << "Functions explicitly expected to not be called:" << std::endl;
		for (std::list<Call *>::const_iterator exp = repo.neverCalls.begin(); exp != repo.neverCalls.end(); ++exp)
			os << **exp;
		os << std::endl;
	}

	if (repo.optionals.size())
	{
		os << "Optional results set up:" << std::endl;
		for (std::list<Call *>::const_iterator exp = repo.optionals.begin(); exp != repo.optionals.end(); ++exp)
			os << **exp;
		os << std::endl;
	}

	return os;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif



#pragma once

#include <algorithm>
#include <numeric>
#include <functional>
#include <iterator>
#include <array>
#include <vector>
#include <utility>
#include <tuple>
#include <memory>

#include <iostream>

namespace pure {

using namespace std;

/*
 * Sequence "[ ]"
 * Any type that defines begin() and end().
 */

namespace cata {

/* 
 * CATEGORIES:
 * In order to easily implement algorithms for different categories of types,
 * we define categories and a type, Cat<T>, which defines Cat<T>::type as
 * either a category or T. This way, we can implement algorithms in terms of
 * either tags (categories) or specific types.
 *
 * This is similar to the tag dispatch pattern used in the STL (see:
 * random_access_iterator_tag, forward_iterator_tag, etc...) except that the
 * type is not erased.
 *
 * The following traits classes merely act as a shortcut to using decltype.
 */

/*
 * []
 * Any type that:
 *      Has a defined begin(s) and end(s).
 */
template< class S > struct sequence { using type = S; };

template< class Seq > struct sequence_traits {
    using sequence   = Seq;
    using iterator   = decltype( begin(declval<Seq>()) );
    using reference  = decltype( *declval<iterator>() );
    using value_type = typename remove_reference<reference>::type;
};

/*
 * Maybe 
 * Any type that: 
 *      Can be dereferenced like so: *Maybe(). (fromJust).
 *      Can be converted to a boolean. (isJust/isNothing)
 *      But is not a function
 */
template< class M > struct maybe { using type = M; };

// If we want to return a pointer that defines ownership, we want a smart
// pointer. Consider any non-raw pointer good 'nuff.
template< class Ptr > struct SmartPtr { using type = Ptr; };
template< class X > struct SmartPtr<X*> { using type = unique_ptr<X>; };

template< class Ptr > struct maybe_traits {
    using pointer    = Ptr;
    using smart_ptr  = typename SmartPtr<pointer>::type;
    using reference  = decltype( *declval<pointer>() );
    using value_type = typename decay<reference>::type;
};

/*
 * cat performs the actual type deduction via its argument. If no other
 * definition of cat is valid, it returns a variable of the same type as its
 * argument. It is only for use in decltype expressions.
 */
template< class X >
X cat( ... );

template< class S >
auto cat( const S& s ) -> decltype( begin(s), end(s), sequence<S>() );

template< class M >
auto cat( const M& m ) -> decltype( *m, (bool)m, maybe<M>() );

template< class T > struct Cat {
    using type = decltype( cat<T>(declval<T>()) );
};

// Prevent function pointers from being deduced as maybe types.
template< class R, class ...X > struct Cat< R(&)(X...) > {
    typedef R(&type)(X...);
};

} // namespace cata

/* 
 * FUNCTION TRANSFORMERS
 * The fallowing are types that contain one or more functions and act like a
 * function. 
 */

/* Translate f g : h(x,xs...) = f( g(x), xs... ) */
template< class F, class G > struct Translate {
    F f;
    G g;

    template< class _F, class _G >
    Translate( _F&& f, _G&& g ) : f(forward<_F>(f)), g(forward<_G>(g)) { }

    template< class X, class ...XS >
    decltype( f(g(declval<X>()),declval<XS>()...) )
    operator() ( X&& x, XS&& ...xs ) {
        return f( g(forward<X>(x)), forward<XS>(xs)... );
    }
};

template< class F, class G >
Translate<F,G> translate( F&& f, G&& g ) {
    return Translate<F,G>( forward<F>(f), forward<G>(g) );
}

/* Flip f : g(x,y) = f(y,x) */
template< class F > struct Flip {
    F f;

    template< class _F >
    Flip( _F&& f ) : f(forward<_F>(f)) { }

    template< class X, class Y, class ...XS >
    decltype( f(declval<Y>(),declval<X>(),declval<XS>()...) )
    operator() ( X&& x, Y&& y, XS&& ...xs ) {
        return f( forward<Y>(y), forward<X>(x), forward<XS>(xs)... );
    }
};

template< class F > Flip<F> flip( F&& f ) {
    return Flip<F>( forward<F>(f) );
}

/* 
 * Partial application.
 * g(y) = f( x, y )
 * partial( f, x ) -> g(y)
 * partial( f, x, y ) -> g()
 */
template< class F, class ...X >
struct PartialApplication;

template< class F, class X >
struct PartialApplication< F, X >
{
    F f;
    X x;

    template< class _F, class _X >
    constexpr PartialApplication( _F&& f, _X&& x )
        : f(forward<_F>(f)), x(forward<_X>(x))
    {
    }

    /* 
     * The return type of F only gets deduced based on the number of xuments
     * supplied. PartialApplication otherwise has no idea whether f takes 1 or 10 xs.
     */
    template< class ... Xs >
    constexpr auto operator() ( Xs&& ...xs )
        -> decltype( f(x,declval<Xs>()...) )
    {
        return f( x, forward<Xs>(xs)... );
    }
};

/* Recursive, variadic version. */
template< class F, class X1, class ...Xs >
struct PartialApplication< F, X1, Xs... > 
    : public PartialApplication< PartialApplication<F,X1>, Xs... >
{
    template< class _F, class _X1, class ..._Xs >
    constexpr PartialApplication( _F&& f, _X1&& x1, _Xs&& ...xs )
        : PartialApplication< PartialApplication<F,X1>, Xs... > (
            PartialApplication<F,X1>( forward<_F>(f), forward<_X1>(x1) ),
            forward<_Xs>(xs)...
        )
    {
    }
};

/* 
 * Some languages implement partial application through closures, which hold
 * references to the function's arguments. But they also often use reference
 * counting. We must consider the scope of the variables we want to apply. If
 * we apply references and then return the applied function, its references
 * will dangle.
 *
 * See: 
 * upward funarg problem http://en.wikipedia.org/wiki/Upward_funarg_problem
 */

/*
 * closure http://en.wikipedia.org/wiki/Closure_%28computer_science%29
 * Here, closure forwards the arguments, which may be references or rvalues--it
 * does not matter. A regular closure works for passing functions down.
 */
template< class F, class ...X >
constexpr PartialApplication<F,X...> closure( F&& f, X&& ...x ) {
    return PartialApplication<F,X...>( forward<F>(f), forward<X>(x)... );
}

/*
 * Thinking as closures as open (having references to variables outside of
 * itself), let's refer to a closet as closed. It contains a function and its
 * arguments (or environment).
 */
template< class F, class ...X >
constexpr PartialApplication<F,X...> closet( F f, X ...x ) {
    return PartialApplication<F,X...>( move(f), move(x)... );
}

/*
 * Reverse partial application. 
 * g(z) = f( x, y, z )
 * rpartial( f, y, z ) -> g(x)
 */
template< class F, class ...X >
struct RPartialApplication;

template< class F, class X >
struct RPartialApplication< F, X > {
    F f;
    X x;

    template< class _F, class _X >
    constexpr RPartialApplication( _F&& f, _X&& x ) 
        : f( forward<_F>(f) ), x( forward<_X>(x) ) { }

    template< class ...Y >
    constexpr decltype( f(declval<Y>()..., x) )
    operator() ( Y&& ...y ) {
        return f( forward<Y>(y)..., x );
    }
};

template< class F, class X1, class ...Xs >
struct RPartialApplication< F, X1, Xs... > 
    : public RPartialApplication< RPartialApplication<F,Xs...>, X1 >
{
    template< class _F, class _X1, class ..._Xs >
    constexpr RPartialApplication( _F&& f, _X1&& x1, _Xs&& ...xs )
        : RPartialApplication< RPartialApplication<F,Xs...>, X1 > (
            RPartialApplication<F,Xs...>( forward<_F>(f), forward<_Xs>(xs)... ),
            forward<_X1>(x1)
        )
    {
    }
};

template< class F, class ...X, class P = RPartialApplication<F,X...> >
constexpr P rclosure( F&& f, X&& ...x ) {
    return P( forward<F>(f), forward<X>(x)... );
}

template< class F, class ...X, class P = RPartialApplication<F,X...> >
constexpr P rcloset( F f, X ...x ) {
    return P( move(f), move(x)... );
}

/* 
 * Composition. 
 * normal notation:  h = f( g() )
 * Haskell notation: h = f . g
 * compose(f,g) -> f . g
 * compose(f,g,h...) -> f . g . h ...
 */

template< class F, class ...G >
struct Composition;

template< class F, class G >
struct Composition<F,G>
{
    F f; G g;

    template< class _F, class _G >
    constexpr Composition( _F&& f, _G&& g ) 
        : f(forward<_F>(f)), g(forward<_G>(g)) 
    {
    }

    template< class ...Args >
    constexpr auto operator() ( Args&& ...args )
        -> decltype( f(g(declval<Args>()...)) )
    {
        return f( g(forward<Args>(args)...) );
    }
};

template< class F, class G, class ...H >
struct Composition<F,G,H...> : Composition<F,Composition<G,H...>>
{
    typedef Composition<G,H...> Comp;

    template< class _F, class _G, class ..._H >
    constexpr Composition( _F&& f, _G&& g, _H&& ...h )
        : Composition<_F,Composition<_G,_H...>> ( 
            forward<_F>(f), 
            Comp( forward<_G>(g), forward<_H>(h)... )
        )
    {
    }
};

template< class F, class ...G >
constexpr Composition<F,G...> compose( F&& f, G&& ...g ) {
    return Composition<F,G...>( forward<F>(f), forward<G>(g)... );
}

template< class F, class ...X >
using Result = decltype( declval<F>()( declval<X>()... ) );

/* map f {1,2,3} -> { f(1), f(2), f(3) } */
template< template<class...> class S, class X, class ...XS, class F,
          class R = Result<F,X> > 
S<R,XS...> map( F&& f, const S<X,XS...>& xs ) 
{
    S<R,XS...> r;
    transform( begin(xs), end(xs), back_inserter(r),
               forward<F>(f) );
    return r;
}

template< class X, size_t N, class F, 
          class R = Result<F,X> >
array< R, N > map( F&& f, const array< X, N >& xs ) {
    array< R, N > r;
    transform( begin(xs), end(xs), begin(r), forward<F>(f) );
    return r;
}

/* foldl f x {1,2,3} -> f(f(f(x,1),2),3) */
template< class F, class X, class S >
constexpr X foldl( F&& f, X&& val, const S& cont )
{
    return accumulate( begin(cont), end(cont), forward<X>(val), 
                       forward<F>(f) );
}

template< class X, class F, class S >
constexpr X foldl( F&& f, const S& cont )
{
    return accumulate( next(begin(cont)), end(cont), 
                            cont.front(), forward<F>(f) );
}

/* foldr f x {1,2,3} -> f(1,f(2,f(3,x))) */
template< class F, class X, class S >
constexpr X foldr( F&& f, X&& val, const S& cont )
{
    return accumulate( cont.rbegin(), cont.rend(), 
                       forward<X>(val), forward<F>(f) );
}

template< class X, class F, class S >
constexpr X foldr( F&& f, const S& cont )
{
    return accumulate( next(cont.rbegin()), cont.rend(), 
                       cont.back(), forward<F>(f) );
}

/* 
 * append({1},{2,3},{4}) -> {1,2,3,4} 
 * Similar to [1]++[2,3]++[4]
 */
template< typename A, typename B >
A append( A a, const B& b ) {
    copy( begin(b), end(b), back_inserter(a) );
    return move(a);
}

template< typename A, typename B, typename C, typename ...D >
A append( A&& a, const B& b, const C& c, const D& ... d )
{
    return append( append(forward<A>(a), b), c, d... );
}


template< class SS, class S = typename SS::value_type >
S concat( const SS& ss ) {
    return foldl( append<S,S>, S(), forward<SS>(ss) );
}

/* 
 * concatMap f s = concat (map f s)
 *      where f is a function: a -> [b]
 *            map f s produces a type: [[b]]
 * The operation "map then concat" may be inefficient compared to an inlined
 * loop, which would not require allocating a sequence of sequences to be
 * concatenated. This is an optimization that should be equivalent to the
 * inlined loop.
 */
template< class F, class S,
          class R = decltype( declval<F>()( declval<S>().front() ) ) > 
R concatMap( F&& f, S&& xs ) {
    return foldr ( 
        // \ acc x = append( acc, f(x) )
        flip( translate(append<R,R>,forward<F>(f)) ), 
        R(), forward<S>(xs)
    );
}

template< typename Container, typename F >
void for_each( F&& f, const Container& cont ) {
    for_each( begin(cont), end(cont), forward<F>(f) );
}

template< class F, class I, class J >
void for_ij( const F& f, I i, const I& imax, J j, const J& jmax ) {
    for( ; j != jmax; j++ )
        for( ; i != imax; i++ )
            f( i, j );
}

template< class F, class I, class J >
void for_ij( const F& f, const I& imax, const J& jmax ) {
    for( J j = J(); j != jmax; j++ )
        for( I i = I(); i != imax; i++ )
            f( i, j );
}


/* 
 * Just  x -> Maybe (Just x) | with a definite value.
 * Nothing -> Maybe Nothing  | with definitely no value.
 */
template< class T, class M = std::unique_ptr<T> > 
constexpr M Just( T t ) {
    return M( new T(move(t)) );
}

template< class X >
constexpr X* Just( X* x ) { return x; }

template< class T, class M = std::unique_ptr<T> > 
constexpr M Nothing() {
    return nullptr;
}

/* maybe b (a->b) (Maybe a) -> b */
template< class R, class F, class P >
constexpr R maybe( R&& nothingVal, F&& f, P&& m ) {
    return m ? forward<F>(f)( *forward<P>(m) )
             : forward<R>( nothingVal );
}

/* 
 * Just f  * Just x  = Just (f x)
 * _       * _       = Nothing
 * Just x  | _       = Just x
 * Nothing | Just x  = Just x
 * Nothing | Nothing = Nothing
 */
template< class F, class P, 
          class Ret = decltype( declval<F>()(*declval<P>()) ) >
auto operator* ( F&& a, P&& b )
    -> unique_ptr< Ret >
{
    return a and b ? Just( (*forward<F>(a))(*forward<P>(b)) ) 
                   : nullptr;
}

template< class P > P operator|| ( P&& a, P&& b ) {
    return a ? forward<P>(a) : forward<P>(b); 
}

/* Either a b : Left a | Right b */
template< class L, class R >
struct Either
{
    typedef L left_type;
    typedef R right_type;

    unique_ptr<left_type> left;
    unique_ptr<right_type> right;

    // TODO: Make these type constructors, not types.
    struct Left { 
        left_type value;
        constexpr Left( left_type value ) : value(move(value)) { }
    };

    struct Right { 
        right_type value;
        constexpr Right( right_type value ) : value(move(value)) { }
    };

    template< class X >
    explicit constexpr Either( X&& x )  
        : left(  new left_type (move(x.value)) ) { }
    explicit constexpr Either( Right x ) 
        : right( new right_type(move(x.value)) ) { }
};

/* 
 * Left<b>  a -> Either a b
 * Right<a> b -> Either a b
 * Since an Either cannot be constructed without two type arguments, the
 * 'other' type must be explicitly declared when called. It is, for
 * convenience, the first type argument in both.
 */
template< class R, class L, class E = Either<L,R> >
constexpr E Left( L x ) { return E( typename E::Left(move(x)) ); }

template< class L, class R, class E = Either<L,R> >
constexpr E Right( R x ) { return E( typename E::Right(move(x)) ); }

/* either (a->c) (b->c) (Either a b) -> c */
template< class F, class G, class L, class R >
constexpr auto either( F&& f, G&& g, const Either<L,R>& e )
    -> decltype( declval<F>()(*e.right) )
{
    return e.right ? forward<F>(f)(*e.right) : forward<G>(g)(*e.left);
}

/*
 * Right f * Right x = Right (f x)
 * _       * _       = Left _
 */
template< class L, class F, class T, 
          class Ret = decltype( declval<F>()(declval<T>()) ) >
constexpr Either<L,Ret> operator* ( const Either<L,F>& a, 
                                    const Either<L,T>& b )
{
    return a.right and b.right ? Right<L>( (*a.right)(*b.right) )
        : a.left ? Left<Ret>( *a.left ) : Left<Ret>( *b.left );
}


/*
 * In category theory, a functor is a mapping between categories.
 * Pure provides the minimal wrappings to lift X to a higher category.
 *
 * pure(x) -> F(x) where F(_) = x
 */
template< class X >
struct Pure
{
    X x;

    explicit constexpr Pure( X x ) : x( x ) { }
    constexpr Pure( Pure&& ) = default;

    template< class ...Args >
    constexpr X operator() ( Args ... ) { return x; }
};

template< class X >
constexpr Pure<X> pure( X&& x )
{
    return Pure<X>( forward<X>(x) );
}

/* 
 * Functor F:
 *      fmap f (F a) -> F (f a)
 *
 * fmap maps a function, f, to a functor, F such that for each f:X->Y, there
 * exists an F(f):F(X)->F(Y). 
 *
 * fmap f F(x) = F(f x) 
 *
 * In Haskell, each mappable type class must be specialized with a Functor
 * instance. Likewise, here, we must use template specialization for each type
 * we want mappable.
 */
template< class ...F > struct Functor;

template< class F, class G, class ...H,
          class Fn = Functor< typename cata::Cat<G>::type > >
constexpr auto fmap( F&& f, G&& g, H&& ...h )
    -> decltype( Fn::fmap(declval<F>(),declval<G>(),declval<H>()...) ) 
{
    return Fn::fmap( forward<F>(f), forward<G>(g), forward<H>(h)... );
}

struct FMap {
    template< class F, class ...X >
    using Result = decltype( fmap(declval<F>(),declval<X>()...) );

    template< class F, class ...X >
    constexpr auto operator() ( F&& f, X&& ...x ) 
        -> decltype( fmap(declval<F>(),declval<X>()...) )
    {
        return fmap( forward<F>(f), forward<X>(x)... );
    }
};

/* to_map x = \f -> fmap f x */
template< class ...X > 
auto to_map( X&& ...x ) -> decltype(rclosure(FMap(),declval<X>()...))
{
    return rclosure( FMap(), forward<X>(x)... );
}


/* fmap f g = compose( f, g ) */
template< class Function >
struct Functor<Function> {
    template< class F, class G >
    static Composition<F,G> fmap( F&& f, G&& g ) {
        return compose( forward<F>(f), forward<G>(g) );
    }
};

/* fmap f Pair(x,y) = Pair( f x, f y ) */
template< class X, class Y >
struct Functor< pair<X,Y> > {
    template< class F, class R = decltype(declval<F>()(declval<X>())) >
    static pair<R,R> fmap( F&& f, const pair<X,Y>& p ) {
        return make_pair( f(p.first), f(p.second) );
    }

    template< class F >
    static pair<X,Y> fmap( F&& f, pair<X,Y>&& p ) {
        F _f = forward<F>(f);
        p.first  = _f( p.first );
        p.second = _f( p.second );
        return move( p );
    }
};

template< class _M >
struct Functor< cata::maybe<_M> > {
    template< class M > 
    static constexpr bool each( const M& m ) {
        return (bool)m;
    }

    template< class M1, class ...Ms >
    static constexpr bool each( const M1& m1, const Ms& ...ms ) {
        return (bool)m1 && each( ms... );
    }

    /*
     * f <$> Just x = Just (f x)
     * f <$> Nothing = Nothing
     */
    template< class F, class ...M,
              class R = decltype( declval<F>()(*declval<M>()...) ) >
    static unique_ptr<R> fmap( F&& f, M&& ...m ) {
        return each( m... ) ? 
            Just( forward<F>(f)( *forward<M>(m)... ) ) : nullptr;
    }
};

/* 
 * Given f(x,y...) and fx(y...)
 *  enclose f = fx
 */
template< class F > struct Enclose {
    F f;

    template< class _F >
    constexpr Enclose( _F&& f ) : f(forward<_F>(f)) { }

    template< class ...X >
    using Result = decltype( closure(f,declval<X>()...) );

    template< class ...X > 
    constexpr Result<X...> operator() ( X&& ...x ) {
        return closure( f, forward<X>(x)... );
    }
};

template< class F, class E = Enclose<F> >
E enclosure( F&& f ) {
    return E( forward<F>(f) );
}

template< class Seq >
struct Functor< cata::sequence<Seq> > {
    /* f <$> [x0,x1,...] = [f x0, f x1, ...] */
    template< class F, class S >
    static decltype( map(declval<F>(),declval<S>()) )
    fmap( F&& f, S&& s ) {
        return map( forward<F>(f), forward<S>(s) );
    }

    template< class F, class ...YS >
    static constexpr auto mapper( F&& f, YS&& ...ys ) 
        -> decltype( compose( to_map(declval<YS>()...), 
                              enclosure(declval<F>()) ) )
    {
        return compose( to_map( forward<YS>(ys)... ),
                        enclosure( forward<F>(f) ) );
    }

    template< class F, class XS, class YS, class ...ZS > 
              //class R = FMap< Part<F,Val<XS>>, ZS > >
    static auto fmap( const F& f, const XS& xs, YS&& ys, ZS&& ...zs ) 
        -> decltype ( 
            mapper(f,declval<YS>(),declval<ZS>()...)(xs.front())
        )

    {
        return concatMap ( 
            mapper( f, forward<YS>(ys), forward<ZS>(zs)... ),
            xs 
        );

        // Alternate:
        //return concatMap ( 
        //    [&]( X x ) {
        //        return fmap( closure(f,x), zs );
        //    },
        //    xs 
        //);
    }
};

template< class L, class R >
struct Functor< Either<L,R> > {
    template< class F, class FR = decltype( declval<F>()(declval<R>()) ) >
    static Either<L,FR>  fmap( F&& f, const Either<L,R>& e ) {
        return e.right ? Right<L>( forward<F>(f)(*e.right) ) 
                       : Left<FR>( *e.left );
    }
};

template< class C > struct IsSeqImpl {
    // Can only be supported on STL-like sequence types, not pointers.
    template< class _C > static true_type f(typename _C::iterator*);
    template< class _C > static false_type f(...);
    typedef decltype( f<C>(0) ) type;
};

template< class C > struct IsSeq : public IsSeqImpl<C>::type { };

/* Enable if is an STL-like sequence. */
template< class C, class R > struct ESeq : enable_if<IsSeq<C>::value,R> { };

/* Disable if is sequence. */
template< class C, class R > struct XSeq : enable_if<not IsSeq<C>::value,R> { };

template< class F, class M >
constexpr decltype( fmap(declval<F>(), declval<M>()) )
operator^ ( F&& f, M&& m ) {
    return fmap( forward<F>(f), forward<M>(m) );
}

/*
 * Monoid M :
 *      mempty -> M
 *      mappend M M -> M
 *      mconcat [M] -> M
 *
 *      mconcat = foldr mappend mempty
 */
template< class ...M > struct Monoid;

template< class M, class Mo = Monoid<typename cata::Cat<M>::type> >
decltype( Mo::mempty() ) mempty() { return Mo::mempty(); }

template< class M1, class M2, 
          class Mo = Monoid<typename cata::Cat<M1>::type> >
decltype( Mo::mappend(declval<M1>(),declval<M2>()) )
mappend( M1&& a, M2&& b ) {
    return Mo::mappend( forward<M1>(a), forward<M2>(b) );
}

/*
 * mappend const& 
 * The above && version will always be preferred, except in the case of a
 * const&. In other words, this version will NEVER be preferred otherwise.
 */
template< class M1, class M2, 
          class Mo = Monoid<typename cata::Cat<M1>::type> >
decltype( Mo::mappend(declval<M1>(),declval<M2>()) )
mappend( const M1& a, M2&& b ) {
    return Mo::mappend( a, forward<M2>(b) );
}

template< class S, class V = typename cata::sequence_traits<S>::value_type,
          class M = Monoid<typename cata::Cat<V>::type> >
decltype( M::mconcat(declval<S>()) ) mconcat( S&& s ) 
{
    return M::mconcat( forward<S>(s) );
}


// When we call mappend from within Monoid<M>, lookup deduction will see its
// own implementation of mappend. This ensures a recursive mappend gets
// properly forwarded.
template< class XS, class YS > 
decltype( mappend(declval<XS>(),declval<YS>()) )
fwd_mappend( XS&& xs, YS&& ys ) {
    return mappend( forward<XS>(xs), forward<YS>(ys) );
}

template< class M > 
decltype( mempty<M>() ) fwd_mempty() { return mempty<M>(); }

template< class Seq > struct Monoid< cata::sequence<Seq> > {
    static Seq mempty() { return Seq{}; }

    template< class XS, class YS >
    static decltype( append(declval<XS>(),declval<YS>()) )
    mappend( XS&& xs, YS&& ys ) {
        return append( forward<XS>(xs), forward<YS>(ys) );
    }

    template< class SS >
    static decltype( concat(declval<SS>()) )
    mconcat( SS&& ss ) {
        return concat( forward<SS>(ss) ); 
    }
};

/* Monoid (Maybe X) -- where X is a monoid. */
template< class M > struct Monoid< cata::maybe<M> > {
    static M mempty() { return nullptr; }

    /*
     * Just x <> Just y = Just (x <> y)
     *      a <> b      = a | b
     */
    static M mappend( const M& x, const M& y ) {
        return x and y ? Just( fwd_mappend(*x,*y) )
            : x ? Just(*x) : y ? Just(*y) : nullptr;
    }

    static M mappend( M&& x, M&& y ) {
        return x and y ? Just( fwd_mappend(*forward<M>(x), *forward<M>(y)) )
            : forward<M>(x) || forward<M>(y);
    }

    /* mconcat [Maybe x] -> Maybe x -- where mappend x x is defined. */
    template< class S > static M mconcat( S&& s ) {
        typedef M (*F) ( const M&, const M& );
        return foldl( (F)mappend, mempty(), forward<S>(s) );
    }
};

/* Monoid (Pair X X) */
template< class X, class Y > struct Monoid< pair<X,Y> > {
    typedef pair<X,Y> P;

    static P mempty() { return P( fwd_mempty<X>(), fwd_mempty<Y>() ); }

    static P mappend( const P& a, const P& b ) {
        return P( fwd_mappend(a.first,b.first), 
                  fwd_mappend(a.second,b.second) );
    }

    template< class S > static P mconcat( const S& s ) {
        return foldr( mappend, mempty(), s );
    }
};

/*
 * Monad M :
 *   mdo (M a) (M b) -> M b        -- (M-do) Do from left to right.
 *   mbind (M a) (a -> M b) -> M b -- Apply to the right the value of the left.
 *   mreturn a -> M a              -- Lift a.
 *   mfail String -> M a           -- Produce a failure value.
 *
 *   a >>  b = mdo   a b
 *   m >>= f = mbind m f
 */
template< class ...M > struct Monad;

/* m >> k */
template< class A, class B,
          class Mo = Monad<typename cata::Cat<A>::type> >
decltype( Mo::mdo(declval<A>(),declval<B>()) ) 
mdo( A&& a, B&& b ) {
    return Mo::mdo( forward<A>(a), forward<B>(b) ); 
}

/* m >>= k */
template< class M, class F,
          class Mo = Monad<typename cata::Cat<M>::type> >
decltype( Mo::mbind(declval<M>(),declval<F>()) ) 
mbind( M&& m, F&& f ) {
    return Mo::mbind( forward<M>(m), forward<F>(f) ); 
}

/* return<M> x = M x */
template< class M, class X > M mreturn( X&& x ) {
    return Monad< typename cata::Cat<M>::type >::mreturn( forward<X>(x) );
}

/* mreturn () = (\x -> return x) */
template< class M, class Mo = Monad<typename cata::Cat<M>::type> > 
decltype( Mo::mreturn() ) mreturn() { 
    return Mo::mreturn();
}

template< class M >
M mfail( const char* const why ) {
    return Monad< typename cata::Cat<M>::type >::mfail( why );
}

template< class X, class Y >
decltype( mbind(declval<X>(),declval<Y>()) ) 
operator >>= ( X&& x, Y&& y ) {
    return mbind( forward<X>(x), forward<Y>(y) );
}

template< class X, class Y >
decltype( mdo(declval<X>(),declval<Y>()) ) 
operator >> ( X&& x, Y&& y ) {
    return mdo( forward<X>(x), forward<Y>(y) );
}

template< class Seq >
struct Monad< cata::sequence<Seq> > {
    template< class X >
    static Seq mreturn( X&& x ) { 
        return Seq{ forward<X>(x) }; 
    }

    static Seq mfail(const char*) { return Seq(); }

    template< class S, class YS >
    static YS mdo( const S& a, const YS& b ) {
        // In Haskell, this is defined as
        //      m >> k = foldr ((++) . (\ _ -> k)) [] m
        // In other words, 
        //      for each element in a, duplicate b.
        //      [] >> k = []
        YS c;
        auto size = a.size();
        while( size-- )
            c = append( c, b );
        return c;
    }

    /* m >>= k -- where m is a cata::sequence. */
    template< class S, class F >
    static decltype( concatMap(declval<F>(),declval<S>()) )
    mbind( S&& xs, F&& f ) { 
        // xs >>= f = foldr g [] xs 
        //     where g acc x = acc ++ f(x)
        //           ++ = append
        return concatMap( f, xs );
    }
};

template< class P > struct IsPointerImpl { 
    using reference = decltype( *declval<P>() );
    using bool_type = decltype( (bool)declval<P>() );
};

template< class M > 
struct Monad< cata::maybe<M> > {
    using traits = cata::maybe_traits<M>;
    using value_type = typename traits::value_type;
    using smart_ptr  = typename traits::smart_ptr;

    template< class X >
    static smart_ptr mreturn( X&& x ) { 
        return smart_ptr( new X(forward<X>(x)) ); 
    }

    using Return = smart_ptr (*) ( value_type );
    static Return mreturn() { return Just; }

    static smart_ptr mfail(const char*) { return nullptr; }

    template< class PZ >
    static PZ mdo( const M& x, PZ&& z ) {
        return x ? forward<PZ>(z) : nullptr;
    }

    template< class F,
              class R = decltype( declval<F>()(declval<value_type>()) ) >
    static R mbind( M&& x, F&& f ) {
        return x ? forward<F>(f)( *forward<M>(x) ) : nullptr;
    }
};

/*
 * MonadPlus M :
 *      mzero -> M
 *      mplus M M -> M
 *
 *      mzero >>= f = mzero
 */
template< class ...F > struct MonadPlus;

template< class M, class Mo = MonadPlus<typename cata::Cat<M>::type> >
decltype( Mo::mzero() ) mzero() { return Mo::mzero(); }

template< class M1, class M2, 
          class Mo = MonadPlus<typename cata::Cat<M1>::type> >
decltype( Mo::mplus(declval<M1>(),declval<M2>()) )
mplus( M1&& a, M2&& b ) {
    return Mo::mplus( forward<M1>(a), forward<M2>(b) );
}

template< class Seq >
struct MonadPlus< cata::sequence<Seq> > {
    static Seq mzero() { return Seq(); }

    template< class SX, class SY >
    static decltype( append(declval<SX>(),declval<SY>()) )
    mplus( SX&& sx, SY&& sy ) {
        return append( forward<SX>(sx), forward<SY>(sy) );
    }
};

template< class M > 
struct MonadPlus< cata::maybe<M> > {
    static M mzero() { return nullptr; }

    template< class A, class B >
    static decltype(declval<A>() || declval<B>())
    mplus( A&& a, B&& b ) { return forward<A>(a) || forward<B>(b); }
};

/*
 * Rotation.
 * f(x...,y) = g(y,x...)
 * rot f = g
 * rrot g = f
 * rrot( rot f ) = f
 *
 * In stack-based (or concatenative languages) rotation applies to the top
 * three elements on the stack. Here, the function's arguments are treated as a
 * stack and rotated. The entire stack gets rotated, rather than the top three
 * elements.
 */
template< class F, class ...Args >
struct PartialLast; // Apply the last argument.

template< class F, class Last >
struct PartialLast< F, Last > : public PartialApplication< F, Last > 
{ 
    template< class _F, class _Last >
    constexpr PartialLast( _F&& f, _Last&& last )
        : PartialApplication<F,Last>( forward<_F>(f), forward<_Last>(last) )
    {
    }
};

// Remove one argument each recursion.
template< class F, class Arg1, class ...Args >
struct PartialLast< F, Arg1, Args... > : public PartialLast< F, Args... > 
{
    template< class _F, class _Arg1, class ..._Args >
    constexpr PartialLast( _F&& f, _Arg1&&, _Args&& ...args )
        : PartialLast<F,Args...>( forward<_F>(f), forward<_Args>(args)... )
    {
    }
};

template< class F, class ...Args >
struct PartialInitial; // Apply all but the last argument.

template< class F, class Arg1, class Arg2 >
struct PartialInitial< F, Arg1, Arg2 > : public PartialApplication< F, Arg1 >
{
    template< class _F, class _Arg1, class _Arg2 >
    constexpr PartialInitial( _F&& f, _Arg1&& arg1, _Arg2&& )
        : PartialApplication<F,Arg1>( forward<_F>(f), forward<_Arg1>(arg1) )
    {
    }
};

template< class F, class Arg1, class ...Args >
struct PartialInitial< F, Arg1, Args... > 
    : public PartialInitial< PartialApplication<F,Arg1>, Args... >
{
    template< class _F, class _Arg1, class ..._Args >
    PartialInitial( _F&& f, _Arg1&& arg1, _Args&& ...args )
        : PartialInitial<PartialApplication<F,Arg1>,Args...> (
            partial( forward<_F>(f), forward<_Arg1>(arg1) ),
            forward<_Args>(args)...
        )
    {
    }
};

template< class F >
struct Rot
{
    F f;

    template< class _F >
    constexpr Rot( _F&& f ) : f( forward<_F>(f) ) { }

    template< class ...Args >
    constexpr auto operator() ( Args&& ...args )
        -> decltype ( 
            declval< PartialInitial<PartialLast<F,Args...>,Args...> >()() 
        )
    {
        /* We can't just (to my knowledge) pull the initial and final args
         * apart, so first reverse-apply the last argument, then apply each
         * argument forward until the last. The result is a zero-arity function
         * to invoke.
         */
        return PartialInitial< PartialLast<F,Args...>, Args... > (
            PartialLast< F, Args... >( f, forward<Args>(args)... ),
            forward<Args>( args )...
        )();
    }
};

template< class F >
struct RRot // Reverse Rotate
{
    F f;

    template< class _F >
    constexpr RRot( _F&& f ) : f( forward<_F>(f) ) { }

    template< class Arg1, class ...Args >
    constexpr auto operator() ( Arg1&& arg1, Args&& ...args )
        -> decltype( f(declval<Args>()..., declval<Arg1>()) )
    {
        return f( forward<Args>(args)..., forward<Arg1>(arg1) );
    }
};

template< class F >
constexpr Rot<F> rot( F&& f )
{
    return Rot<F>( forward<F>(f) );
}

template< class F >
constexpr RRot<F> rrot( F&& f )
{
    return RRot<F>( forward<F>(f) );
}

/* Rotate F by N times. */
template< unsigned int N, class F >
struct NRot;

template< class F >
struct NRot< 1, F > : public Rot<F>
{
    template< class _F >
    constexpr NRot( _F&& f ) : Rot<F>( forward<_F>(f) ) { }
};

template< unsigned int N, class F >
struct NRot : public NRot< N-1, Rot<F> >
{
    template< class _F >
    constexpr NRot( _F&& f ) : NRot< N-1, Rot<F> >( rot(forward<_F>(f)) ) { }
};

template< unsigned int N, class F >
struct RNRot; // Reverse nrot.

template< class F >
struct RNRot< 1, F > : public RRot<F>
{
    template< class _F >
    RNRot( _F&& f ) : RRot<F>( forward<_F>(f) ) { }
};

template< unsigned int N, class F >
struct RNRot : public RNRot< N-1, RRot<F> >
{
    template< class _F >
    RNRot( _F&& f ) : RNRot<N-1,RRot<F>>( rrot(forward<_F>(f)) ) { }
};

template< unsigned int N, class F >
constexpr NRot<N,F> nrot( F&& f )
{
    return NRot<N,F>( forward<F>(f) );
}

template< unsigned int N, class F >
constexpr RNRot<N,F> rnrot( F&& f )
{
    return RNRot<N,F>( forward<F>(f) );
}

/* squash[ f(x,x) ] = g(x) */
template< class F >
struct Squash
{
    F f;

    template< class _F >
    constexpr Squash( _F&& f ) : f( forward<_F>(f) ) { }

    template< class Arg1, class ...Args >
    constexpr auto operator() ( Arg1&& arg1, Args&& ...args )
        -> decltype( f(declval<Arg1>(),declval<Arg1>(),declval<Args>()...) )
    {
        return f( forward<Arg1>(arg1), forward<Arg1>(arg1), 
                  forward<Args>(args)... );
    }
};

template< class F >
constexpr Squash<F> squash( F&& f )
{
    return Squash<F>( forward<F>(f) );
}

/* 
 * f( x, y ) = f( l(z), r(z) ) = g(z)
 *  where x = l(z) and y = r(z)
 * join( f, l, r ) = g
 */
template< class F, class Left, class Right >
struct Join
{
    F f;
    Left l;
    Right r;

    template< class _F, class _L, class _R >
    constexpr Join( _F&& f, _L&& l, _R&& r )
        : f(forward<_F>(f)), l(forward<_L>(l)), r(forward<_R>(r))
    {
    }

    template< class A, class ...AS >
    constexpr auto operator() ( A&& a, AS&& ...as )
        -> decltype( f(l(declval<A>()),r(declval<A>()),declval<AS>()...) )
    {
        return f( l(forward<A>(a)), r(forward<A>(a)), forward<AS>(as)... );
    }
};

template< class F, class L, class R >
constexpr Join<F,L,R> join( F&& f, L&& l, R&& r )
{
    return Join<F,L,R>( forward<F>(f), forward<L>(l), forward<R>(r) );
}

template< typename Container >
constexpr bool ordered( const Container& c )
{
    return c.size() <= 1
        or mismatch ( 
            begin(c), end(c)-1, 
            begin(c)+1, 
            less_equal<int>() 
        ).second == end(c);
}

/* filter f C -> { x for x in C such that f(x) is true. } */
template< typename Container, typename F >
Container filter( F&& f, Container cont )
{
    cont.erase ( 
        remove_if( begin(cont), end(cont), 
                   compose( [](bool b){return !b;}, forward<F>(f) ) ),
        end( cont )
    );
    return cont;
}

/* find pred xs -> Maybe x */
template< class F, class S,
          class Val = decltype( &( * begin(declval<S>()) ) ) >
Val find( F&& f, const S& s )
{
    const auto& e = end(s);
    const auto it = find_if( begin(s), e, forward<F>(f) );
    return it != e ? &(*it) : nullptr; 
}

/* cfind x C -> C::iterator */
template< typename S, typename T >
constexpr auto cfind( T&& value, const S& cont )
    -> decltype( begin(cont) )
{
    return find( begin(cont), end(cont), forward<T>(value) );
}

template< typename S, typename F >
constexpr auto cfind_if( F&& f, const S& cont )
    -> decltype( begin(cont) )
{
    return find_if( begin(cont), end(cont), forward<F>(f) );
}

/* all f C -> true when f(x) is true for all x in C; otherwise false. */
template< typename Container, typename F >
constexpr bool all( F&& f, const Container& cont )
{
    return all_of( begin(cont), end(cont), forward<F>(f) );
}

/* zip_with f A B -> { f(a,b) for a in A and b in B } */
template< typename Container, typename F >
Container zip_with( F&& f, const Container& a, Container b )
{
    transform( begin(a), end(a), begin(b), 
               begin(b), forward<F>(f) );
    return b;
}

/*
 * cleave x f g h -> { f(x), g(x), h(x) }
 * Inspired by Factor, the stack-based language.
 */
template< typename X, typename F, typename ... Fs,
          typename R = decltype( declval<F>()(declval<X>()) ) >
constexpr array<R,sizeof...(Fs)+1> cleave( X&& x, F&& f, Fs&& ... fs ) 
{
    return {{ forward<F> (f )( forward<X>(x) ), 
              forward<Fs>(fs)( forward<X>(x) )... }};
}

/* cleave_with f x y z =  { f(x), f(y), f(z) } */
template< class F, class A, class ...B >
constexpr auto cleave_with( F&& f, A&& a, B&& ...b )
    -> array< decltype( declval<F>()(declval<A>()) ), sizeof...(B)+1 > 
{
    return {{ f(forward<A>(a)), f(forward<B>(b))... }};
}

template< class T, unsigned int N, class F >
array<T,N> generate( F&& f ) {
    array<T,N> cont;
    generate( begin(cont), end(cont), forward<F>(f) );
    return cont;
}

template< class T, class F >
vector<T> generate( F&& f, unsigned int n ) {
    vector<T> c; 
    c.reserve(n);
    while( n-- )
        c.push_back( forward<F>(f)() );
    return c;
}

template< class Cmp, class S, class R = decltype( begin(declval<Cmp>()) ) >
constexpr R max( Cmp&& cmp, const S& cont ) {
    return max_element( begin(cont), end(cont), forward<Cmp>(cmp) );
}

template< class S >
constexpr auto max( const S& cont ) -> decltype( begin(cont) ) {
    return max_element( begin(cont), end(cont) );
}

template< class Cmp, class S >
constexpr auto min( Cmp&& cmp, const S& cont ) -> decltype( begin(cont) ) {
    return min_element( begin(cont), end(cont), forward<Cmp>(cmp) );
}

template< class Container >
constexpr auto min( const Container& cont )
    -> decltype( begin(cont) )
{
    return min_element( begin(cont), end(cont) );
}

} // namespace pure



#pragma once

#include "Functional.h"

namespace pure {

namespace category {

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

struct other {};

/*
 * []
 * Any type that:
 *      Has a defined begin(s) and end(s).
 */
struct sequence_type {};

/*
 * Maybe 
 * Any type that: 
 *      Can be dereferenced like so: *Maybe(). (fromJust).
 *      Can be converted to a boolean. (isJust/isNothing)
 *      But is not a function
 */
struct maybe_type {};

// If we want to return a pointer that defines ownership, we want a smart
// pointer. Consider any non-raw pointer good 'nuff.
template< class Ptr > struct SmartPtr { using type = Ptr; };
template< class X > struct SmartPtr<X*> { using type = std::unique_ptr<X>; };

template< class Ptr > struct maybe_traits {
    using pointer    = Ptr;
    using smart_ptr  = typename SmartPtr<pointer>::type;
    using reference  = decltype( *declval<pointer>() );
    using value_type = Decay<reference>;
};

/*
 * cat performs the actual type deduction via its argument. If no other
 * definition of cat is valid, it returns a variable of the same type as its
 * argument. It is only for use in decltype expressions.
 */
template< class X >
X cat( ... );

template< class S >
auto cat( const S& s ) -> decltype( begin(s), end(s), sequence_type() );

template< class M >
auto cat( const M& m ) -> decltype( *m, (bool)m, maybe_type() );

template< class T > struct CatImpl {
    using type = decltype( cat<Decay<T>>(declval<T>()) );
};

// Prevent function pointers from being deduced as maybe_type types.
template< class R, class ...X > struct CatImpl< R(&)(X...) > {
    typedef R(&type)(X...);
};

template< class X >
using Cat = typename CatImpl<X>::type;

template< class ... > struct Category;

template< class F, class X, class C = Category< Cat<F> > >
auto cid( X&& x ) -> decltype( C::template id< Cat<F> >( declval<X>() ) ) {
    return C::template id< Cat<F> >( forward<X>(x) );
}

struct CId {
    template< class X >
    constexpr X operator() ( X&& x ) { return cid( forward<X>(x) ); }
};

constexpr struct Comp : Chainable<Comp> {
    using Chainable<Comp>::operator();

    /* Let comp be a generalization of compose. */
    template< class F, class G, class C = Category< Cat<F> > >
    constexpr auto operator () ( F&& f, G&& g )
        -> decltype( C::comp( declval<F>(), declval<G>() ) )
    {
        return C::comp( forward<F>(f), forward<G>(g) );
    }
} comp{};

constexpr struct FComp : Binary<FComp> {
    using Binary<FComp>::operator();

    /* comp reversed. */
    template< class F, class G >
    constexpr auto operator () ( F&& f, G&& g )
        -> decltype( comp(declval<G>(), declval<F>()) )
    {
        return comp( forward<G>(g), forward<F>(f) );
    }

    template< class F, class G, class H, class ...I>
    constexpr auto operator () ( F&& f, G&& g, H&& h, I&& ...i )
        -> decltype( comp( (*this)(declval<G>(),declval<H>(),declval<I>()...),
                     declval<F>() ) )
    {
        return comp( (*this)( forward<G>(g), forward<H>(h), forward<I>(i)... ),
                     forward<F>(f) );
    }
} fcomp{};

/* Default category: function. */
template< class F > struct Category<F> {
    static constexpr Id id = Id();

    template< class G >
    static constexpr G io( G&& g ) {
        return std::forward<G>(g);
    }

    static constexpr auto comp = compose;
};


/* f > g = g . f (Haskell's >>>.) */
template< class F, class G >
constexpr auto operator > ( F&& f, G&& g )
    -> decltype( comp(declval<G>(),declval<F>()) )
{
    return comp( forward<G>(g), forward<F>(f) );
}

/* f < g = f . g (Haskell's <<<.) */
template< class F, class G >
constexpr auto operator < ( F&& f, G&& g )
    -> decltype( comp(declval<F>(),declval<G>()) )
{
    return comp( forward<F>(f), forward<G>(g) );
}

} // namespace cata

} // namespace pure

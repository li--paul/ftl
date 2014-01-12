/*
 * Copyright (c) 2013 Björn Aili
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 */
#ifndef FTL_SUM_TYPE_H
#define FTL_SUM_TYPE_H

#include <stdexcept>
#include <memory>
#include <string>
#include "type_functions.h"
#include "type_traits.h"

namespace ftl {

	/**
	 * \defgroup sum_type Sum Type
	 *
	 * Sum types, related functions and concepts.
	 *
	 * \code
	 *   #include <ftl/sum_type.h>
	 * \endcode
	 *
	 * \par Dependencies
	 * - `<stdexcept>`
	 * - `<memory>`
	 * - `<string>`
	 * - \ref typelevel
	 * - \ref typetraits
	 */

	/**
	 * Exception type signifying an attempt to access an empty maybe object.
	 *
	 * \ingroup sum_type
	 */
	class invalid_sum_type_access : public std::logic_error {
	public:
		explicit invalid_sum_type_access(const std::string& what)
		: logic_error(what) {}

		explicit invalid_sum_type_access(std::string&& what)
		: logic_error(std::move(what)) {}

		explicit invalid_sum_type_access(const char* what)
		: logic_error(what) {}

	};

	/**
	 * Type tag used to construct which constructor to use in a `sum_type`.
	 *
	 * \par Examples
	 *
	 * Copy-constructing a sub-type of a `sum_type`
	 * \code
	 *   sum_type<int,string> x{constructor<int>(), 12};
	 * \endcode
	 *
	 * Calling arbitrary constructor of a sub-type
	 * \code
	 *   // B must have a constructor taking an int and a c-string, or something
	 *   // they implicitly convert to
	 *   sum_type<A,B> x{constructor<B>(), 12, "foo"};
	 * \endcode
	 *
	 * \see sum_type
	 *
	 * \ingroup sum_type
	 */
	template<typename>
	struct constructor {
	};

	/**
	 * Thin type wrapper used to distinguish match cases of convertible types
	 *
	 * The `case_` type is explicitly constructible from `T` and implicitly
	 * converts back to it. As `sum_type::match` always passes values by first
	 * wrapping them in a `case_`, this allows match functions to enforce strong
	 * typing when passing the value along, while allowing the user to rely on
	 * implicit conversion to get the actual value out.
	 *
	 * \par Examples
	 *
	 * Matching two implicitly converting sub-types
	 * \code
	 *   sum_type<int,char> x = ...;
	 *   bool isInt = x.match(
	 *       [](case_<int> x){ return true; },
	 *       [](case_<char> x){ return false; }
	 *   );
	 * \endcode
	 *
	 * \see sum_type::match
	 *
	 * \ingroup sum_type
	 */
	template<typename T>
	struct case_ {
		explicit constexpr case_(T t)
		noexcept(std::is_nothrow_copy_constructible<T>::value)
		: t(t) {}

		constexpr operator T() const noexcept {
			return t;
		}

		T t;
	};

	namespace _dtl {

		template<typename, typename...>
		struct is_type_set;

		template<typename T>
		struct is_type_set<T> {
			static constexpr bool value = true;
		};

		template<typename T, typename U, typename...Ts>
		struct is_type_set<T,U,Ts...> {
			static constexpr bool value =
				is_type_set<T,Ts...>::value
				&& is_type_set<U,Ts...>::value;
		};

		template<typename T, typename...Ts>
		struct is_type_set<T,T,Ts...> {
			static constexpr bool value = false;
		};

		template<typename,typename...>
		struct find_call_match {
			using type = _dtl::no;
		};

		template<typename T, typename F, typename...Fs>
		struct find_call_match<T,F,Fs...> {
			using type = if_<is_callable<F,T>::value,
				  typename is_callable<F,T>::type,
				  typename find_call_match<T,Fs...>::type
			>;
		};

		template<typename,typename>
		struct all_return_types;

		template<typename...Fs>
		struct all_return_types<type_seq<>,type_seq<Fs...>> {
			using types = type_seq<>;
		};

		template<typename T, typename...Ts, typename...Fs>
		struct all_return_types<type_seq<T,Ts...>,type_seq<Fs...>> {
			using types =
				typename concat_type_seqs<
					type_seq<typename find_call_match<case_<T>,Fs...>::type>,
					typename all_return_types<
						type_seq<Ts...>,
						type_seq<Fs...>
					>::types
				>::type;
		};

		template<typename>
		struct find_common_type;

		template<typename...Ts>
		struct find_common_type<type_seq<Ts...>> {
			using type = typename std::common_type<Ts...>::type;
		};

		template<typename Ts,typename Fs>
		struct common_return_type {
			using types = typename all_return_types<Ts,Fs>::types;
			using type = typename find_common_type<types>::type;
		};

		template<typename,typename>
		struct exhaustive_match;

		// A match is exhaustive if we've functions to spare
		template<typename...Fs>
		struct exhaustive_match<type_seq<Fs...>,type_seq<>> {
			static constexpr bool value = true;
		};

		// A match is exhaustive if the final function accepts the final type
		template<typename F, typename T>
		struct exhaustive_match<type_seq<F>,type_seq<T>> {
			static constexpr bool value = is_callable<F,case_<T>>::value;
		};

		// A match is NOT exhaustive if there are left-over types
		template<typename T, typename...Ts>
		struct exhaustive_match<type_seq<>,type_seq<T,Ts...>> {
			static constexpr bool value = false;
		};

		template<typename F, typename...Fs, typename T, typename...Ts>
		struct exhaustive_match<type_seq<F,Fs...>,type_seq<T,Ts...>> {
			static constexpr bool value =
				(
					is_callable<F,case_<T>>::value
					&& exhaustive_match<type_seq<Fs...>,type_seq<Ts...>>::value
				)
				||
				(
					exhaustive_match<type_seq<F,Fs...>,type_seq<Ts...>>::value
					&& exhaustive_match<type_seq<Fs...>,type_seq<T>>::value
				);
		};

		template<typename...>
		struct recursive_union {};

		template<size_t I, typename T, typename...Ts>
		struct union_indexer {
			static constexpr auto ref(recursive_union<T,Ts...>& u)
			-> decltype(union_indexer<I-1,Ts...>::ref(u.r)) {
				return union_indexer<I-1,Ts...>::ref(u.r);
			}

			static constexpr auto ref(const recursive_union<T,Ts...>& u)
			-> decltype(union_indexer<I-1,Ts...>::ref(u.r)) {
				return union_indexer<I-1,Ts...>::ref(u.r);
			}

			static constexpr auto ptr(recursive_union<T,Ts...>& u)
			-> decltype(union_indexer<I-1,Ts...>::ptr(u.r)) {
				return union_indexer<I-1,Ts...>::ptr(u.r);
			}
		};

		template<typename T, typename...Ts>
		struct union_indexer<0,T,Ts...> {
			static constexpr T& ref(recursive_union<T,Ts...>& u) {
				return u.v;
			}

			static constexpr const T& ref(const recursive_union<T,Ts...>& u) {
				return u.v;
			}

			static constexpr T* ptr(recursive_union<T,Ts...>& u) {
				return std::addressof(u.v);
			}
		};

		template<typename T>
		struct overload_tag {};

		template<typename R, typename T, typename...>
		struct union_visitor {
			template<
				typename O, typename F, typename...Fs,
				typename = typename std::enable_if<
					is_callable<F,case_<T>>::value
				>::type
			>
			static R visit(overload_tag<O>, T& t, F&& f, Fs&&...) {
				return std::forward<F>(f)(case_<T>{t});
			}

			template<
				typename F, typename O, typename...Fs,
				typename = typename std::enable_if<
					!is_callable<F,case_<T>>::value
				>::type
			>
			static R visit(overload_tag<O> o, T& t, F&&, Fs&&...fs) {
				return union_visitor::visit(o, t, std::forward<Fs>(fs)...);
			}
		};

		template<typename R, typename...Ts>
		struct union_visitor<R,seq<>,Ts...> {
			template<typename...Fs>
			static R visit(recursive_union<Ts...>&, size_t, Fs&&...) {
				throw invalid_sum_type_access{""};
			}
		};

		template<typename R, size_t I, size_t...Is, typename T, typename...Ts>
		struct union_visitor<R,seq<I,Is...>,T,Ts...> {
			template<typename...Fs>
			static R visit(
					recursive_union<T,Ts...>& u, size_t i, Fs&&...fs
			) {
				if(i == I) {
					return union_visitor<R,T>::visit(
						overload_tag<T>{}, u.v, std::forward<Fs>(fs)...
					);
				}
				else {
					return union_visitor<R,seq<Is...>,Ts...>::visit(
						u.r, i, std::forward<Fs>(fs)...
					);
				}
			}
		};

		template<>
		struct recursive_union<> {
			void copy(size_t) noexcept {}
			void move(size_t) noexcept {}
			void destruct(size_t) noexcept {}
		};

		template<typename T, typename...Ts>
		struct recursive_union<T,Ts...> {
			template<typename...Args>
			explicit constexpr recursive_union(constructor<T>, Args&&...args)
			noexcept(std::is_nothrow_constructible<T,Args...>::value)
			: v(std::forward<Args>(args)...) {}

			template<typename U, typename...Args>
			explicit constexpr recursive_union(constructor<U> t, Args&&...args)
			noexcept(
				std::is_nothrow_constructible<
					recursive_union<Ts...>,Args...
				>::value
			)
			: r(t, std::forward<Args>(args)...) {}

			void copy(size_t i, const recursive_union& u)
			noexcept(
				std::is_nothrow_copy_constructible<T>::value
				&& noexcept(std::declval<recursive_union>().r.copy(i-1, u.r))
			)
			{
				if(i == 0) {
					new (std::addressof(v)) T(u.v);
				}
				else {
					r.copy(i-1, u.r);
				}
			}

			void move(size_t i, recursive_union&& u)
			noexcept(
				std::is_nothrow_move_constructible<T>::value
				&& noexcept(std::declval<recursive_union>().r.move(i-1, u.r))
			)
			{
				if(i == 0) {
					new (std::addressof(v)) T(std::move(u.v));
				}
				else {
					r.move(i-1, std::move(u.r));
				}
			}

			void destruct(size_t i)
			noexcept(
				std::is_nothrow_destructible<T>::value
				&& noexcept(std::declval<recursive_union>().r.destruct(i))
			)
			{
				if(i == 0)
				{
					v.~T();
				}
				else {
					r.destruct(i-1);
				}
			}

			union {
				T v;
				recursive_union<Ts...> r;
			};
		};

		template<size_t I, typename...Ts>
		class get_sum_type_element;
	}

	/**
	 * A basic sum type with machinery for pattern matching.
	 *
	 * A sum type is essentially a tagged union. It encompasses the idea of a
	 * value that is either of type `T1`, or `T2`, or, ..., `TN` (for
	 * the sum type `T1+T2+...+TN`, but it may never have a value of more than
	 * one of its sub-types at a time.
	 *
	 * In FTL, this is enforced as much as is possible at compile time, but for
	 * many operations that is not really an option. That's why it's
	 * recommended to stick with operations that can be statically verified,
	 * e.g. using `match` to access values instead of `get`.
	 *
	 * \par Concepts
	 * - \ref copycons, if all sub-types are
	 * - \ref movecons, if all sub-types are
	 *
	 * \ingroup sum_type
	 */
	template<typename...Ts>
	class sum_type {
		static_assert(
			_dtl::is_type_set<Ts...>::value,
			"Each sub-type of a sum type must be unique"
		);

		template<size_t I, typename...Us>
		friend class ::ftl::_dtl::get_sum_type_element;

	public:
		sum_type() = delete;
		sum_type(const sum_type& st) : cons(st.cons) {
			data.copy(cons, st.data);
		}

		sum_type(sum_type&& st) : cons(st.cons) {
			data.move(cons, std::move(st.data));
		}

		template<typename T, typename...Args>
		explicit constexpr sum_type(constructor<T> t, Args&&...args)
		noexcept(
			std::is_nothrow_constructible<
				_dtl::recursive_union<Ts...>,Args...
			>::value
		)
		: data(t, std::forward<Args>(args)...)
		, cons(index_of<T,Ts...>::value) {}

		~sum_type() noexcept(
			noexcept(std::declval<_dtl::recursive_union<Ts...>>().destruct(0))
		)
		{
			data.destruct(cons);
		}

		/**
		 * Pseudo pattern match method.
		 *
		 * Allows a pattern match-like syntax for working with sum type values.
		 * The case clauses are statically enforced to cover all the types that
		 * may be held by the sum type.
		 *
		 * The match call will return the common type (as found by
		 * `std::common_type`) of all the given case clauses' return types.
		 *
		 * \par Examples
		 *
		 * Matching values whose type cannot be implicitly converted
		 * \code
		 *   sum_type<A,B,C> value = ...;
		 *   auto str = value.match(
		 *       [](A a){ return std::string("A"); },
		 *       [](B b){ return std::string("B"); },
		 *       [](C c){ return std::string("C"); }
		 *   );
		 * \endcode
		 */
		template<typename...Fs>
		auto match(Fs&&...fs) -> typename ::ftl::_dtl::common_return_type<
			type_seq<Ts...>,type_seq<Fs...>
		>::type {

			static_assert(
				_dtl::exhaustive_match<type_seq<Fs...>,type_seq<Ts...>>::value,
				"Match statements must be exhaustive"
			);

			using indices = typename gen_seq<0,sizeof...(Ts)-1>::type;
			using return_type = typename _dtl::common_return_type<
				type_seq<Ts...>,type_seq<Fs...>
			>::type;

			return _dtl::union_visitor<return_type,indices,Ts...>
				::visit(data, cons, std::forward<Fs>(fs)...);
		}

	private:
		_dtl::recursive_union<Ts...> data;
		size_t cons;
	};

	namespace _dtl {
		template<size_t I, typename...Ts>
		class get_sum_type_element {
		public:
			// TODO: C++14: With relaxed constexpr requirements,
			// these are possible candidates
			static auto get(sum_type<Ts...>& u)
			-> decltype(union_indexer<I,Ts...>::ref(u.data)) {
				if(u.cons != I)
					throw invalid_sum_type_access{
						std::string("Indexing with ")
						+ std::to_string(I)
						+ std::string(", but active index is ")
						+ std::to_string(u.cons)
					};

				return union_indexer<I,Ts...>::ref(u.data);
			}

			static auto get(const sum_type<Ts...>& u)
			-> decltype(union_indexer<I,Ts...>::ref(u.data)) {
				if(u.cons != I)
					throw invalid_sum_type_access{
						std::string("Indexing with ")
						+ std::to_string(I)
						+ std::string(", but active index is ")
						+ std::to_string(u.cons)
					};

				return union_indexer<I,Ts...>::ref(u.data);
			}
		};
	}

	/**
	 * Means of accessing a sum type by index.
	 *
	 * If the given index does not match the type contained in the sum type at
	 * run-time, an `invalid_sum_type_access`-exception will be thrown.
	 *
	 * \note This is _not_ the recommended way of accessing sum type values. Use
	 *       `sum_type::match` whenever possible instead, to get compile time
	 *       guarantees of not accessing the wrong value.
	 *
	 * \par Examples
	 *
	 * \code
	 *   sum_type<int,float> x{constructor<int>(), 12};
	 *   x.get<0>() += 1;
	 *   // x.get<0>() == 13
	 * \endcode
	 *
	 * \ingroup sum_type
	 */
	template<size_t I, typename...Ts>
	type_at<I,Ts...>& get(sum_type<Ts...>& x) {
		return ::ftl::_dtl::get_sum_type_element<I,Ts...>::get(x);
	}

	/// \overload
	template<size_t I, typename...Ts>
	const type_at<I,Ts...>& get(const sum_type<Ts...>& x) {
		return ::ftl::_dtl::get_sum_type_element<I,Ts...>::get(x);
	}
}

#endif


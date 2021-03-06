#ifndef __UTILS_H__
#define __UTILS_H__

#include "include_headers.h"
#include "include_graphs.h"
#include "GSLwrappers.h"
#include <gsl/gsl_sf_log.h>


namespace utils{

	using MatRow  	= Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
	using IdxType   = std::size_t;
	using Container = std::vector<unsigned int>;
	using MatCol   	= Eigen::MatrixXd;
	using VecCol    = Eigen::VectorXd;
	using ArrInt    = Eigen::Array<unsigned int, Eigen::Dynamic, 1>;
	using VecInt    = Eigen::Matrix<unsigned int, Eigen::Dynamic, 1>;

	//Some constants
	inline constexpr double pi 			     = M_PI;
	inline constexpr double pi_2 			 = M_PI_2;
	inline constexpr double sqrt_2 		     = M_SQRT2;
	inline constexpr double two_over_sqrtpi  = M_2_SQRTPI;
	inline constexpr long double log_2	 	 = M_LN2;
	inline constexpr double sqrt_pi 		 = two_over_sqrtpi*pi_2;
	inline constexpr long double sqrt_2pi 	 = sqrt_pi*sqrt_2;
	inline constexpr long double log_2pi	 = std::log(2*pi);
	inline constexpr long double log_pi	 	 = std::log(pi);

  	//------------------------------------------------------------------------------------------------------------------------------------------------------
	//With the introduction of Eigen 3.4, all these operations can be done much easier and faster. Unfortunately right now RcppEigen supports only Eigen
	// 3.3.7.
	// See https://eigen.tuxfamily.org/dox-devel/group__TutorialSlicingIndexing.html
	enum class Symmetric{
		True, //Assuming it is upper symmetric
		False
	};
	// The following functor and function are taken from the Eigen manual. https://eigen.tuxfamily.org/dox/TopicCustomizing_NullaryExpr.html.
	// The implement a way to easily select submatrices in Matlab fashion, i.e A([1,2,5],[0,3,6]). They are much more general with respect to SubMatrix_old and
	// way more options are available. For the needs of rgwish_core they actually do not perform much better than the old version.
	// Right now there is a problem, Neighbourhood is map< vector<unsigned int> > which means that all the nbds need to be mapped into an eigen type. The overhead of Eigen::Map
	// should be minimal but that function directly works on the buffer. This means that it is not possible to declare nbds as const. --> that was solved
	// Moreover, removing the const adornal, it is 
	// now no longer possible to pass r-value references, i.e SubMatrix({0,1},M) is not possible.
	template<class ArgType, class RowIndexType, class ColIndexType>
	class indexing_functor {
	  const ArgType &m_arg;
	  const RowIndexType &m_rowIndices;
	  const ColIndexType &m_colIndices;
	  public:
	  typedef Eigen::Matrix<typename ArgType::Scalar,
	                 		RowIndexType::SizeAtCompileTime,
	                 		ColIndexType::SizeAtCompileTime,
	                 		ArgType::Flags&Eigen::RowMajorBit?Eigen::RowMajor:Eigen::ColMajor,
	                 		RowIndexType::MaxSizeAtCompileTime,
	                 		ColIndexType::MaxSizeAtCompileTime> MatrixType;
	 
	  indexing_functor(const ArgType& arg, const RowIndexType& row_indices, const ColIndexType& col_indices)
	    : m_arg(arg), m_rowIndices(row_indices), m_colIndices(col_indices)
	  {}
	 
	  const typename ArgType::Scalar& operator() (Eigen::Index row, Eigen::Index col) const {
	    return m_arg(m_rowIndices[row], m_colIndices[col]);
	  }
	};
	template <class ArgType, class RowIndexType, class ColIndexType>
	Eigen::CwiseNullaryOp<indexing_functor<ArgType,RowIndexType,ColIndexType>, typename indexing_functor<ArgType,RowIndexType,ColIndexType>::MatrixType>
	indexing(const Eigen::MatrixBase<ArgType>& arg, const RowIndexType& row_indices, const ColIndexType& col_indices)
	{
	  typedef indexing_functor<ArgType,RowIndexType,ColIndexType> Func;
	  typedef typename Func::MatrixType MatrixType;
	  return MatrixType::NullaryExpr(row_indices.size(), col_indices.size(), Func(arg.derived(), row_indices, col_indices));
	}

	template<Symmetric Sym = Symmetric::False> 
	MatRow SubMatrix(Container const & nbd, MatRow const & M){
		//Check
		if(M.rows() != M.cols())
			throw std::runtime_error("Passing different number of rows and cols in a symmetric matrix. Maybe you need to use Symmetric::False");
		if(*nbd.crbegin() >= M.rows())
			throw std::runtime_error("Indeces exceed matrix dimension");

		MatRow res = indexing(M, Eigen::Map<const ArrInt> (&(nbd[0]), nbd.size()), Eigen::Map<const ArrInt> (&(nbd[0]), nbd.size()));
		
		if constexpr(Sym == Symmetric::True){
			return res.selfadjointView<Eigen::Upper>();
		}
		else{
			return res;
		}
	} //Gets vector whose rows and cols has to be estracted
	template<Symmetric Sym = Symmetric::False>
	MatRow SubMatrix(unsigned int const & exclude, MatRow const & M){
		//Check
		if(M.cols() != M.rows())
			throw std::runtime_error("Non square matrix inserted.");
		if(exclude >= M.rows())
			throw std::runtime_error("Index exceed matrix dimension");
		const unsigned int & N = M.rows();
		//Fill the sub-matrix
		MatRow res(MatRow::Zero(N-1, N-1));
		if(exclude == 0){
			ArrInt nbd(ArrInt::LinSpaced(N-1, 1, N));
			res = indexing(M, nbd, nbd );
		}
		else if(exclude == N-1){
			ArrInt nbd(ArrInt::LinSpaced(N-1, 0, N-1 ));
			res = indexing(M, nbd, nbd );
		}
		else{
			ArrInt nbd(N-1);
			nbd.head(exclude) = ArrInt::LinSpaced(exclude, 0, exclude-1);
			nbd.tail(N - 1 - exclude) = ArrInt::LinSpaced(N - 1 - exclude, exclude+1, N);
			res = indexing(M, nbd, nbd );
		}
		if constexpr(Sym == Symmetric::True)
			return res.selfadjointView<Eigen::Upper>();
		else
			return res;
	} //gets the index whose row and column has to be excluded. Symmetry here is not for efficiency but to be sure to get a sym matrix
	MatRow SubMatrix(Container const & nbd_rows, Container const & nbd_cols, MatRow const & M){
		if(*nbd_rows.crbegin() >= M.rows() || *nbd_cols.crbegin() >= M.cols())
			throw std::runtime_error("Indeces exceed matrix dimension");
		MatRow res = indexing(M, Eigen::Map<const ArrInt> (&(nbd_rows[0]), nbd_rows.size()), Eigen::Map<const ArrInt> (&(nbd_cols[0]), nbd_cols.size()));
		return res;
	} //Passing rows to be exctracted and cols to be extracted. May be different
	MatRow SubMatrix(Container const & nbd_rows, unsigned int idx, MatRow const & M){
		if(*nbd_rows.crbegin() >= M.rows() || idx >= M.cols())
			throw std::runtime_error("Indeces exceed matrix dimension");
		MatRow res = indexing( M, Eigen::Map<const ArrInt> (&(nbd_rows[0]), nbd_rows.size()), Eigen::Map<const ArrInt> (&idx, 1) );
		return res;
	} //Passing rows to be exctracted and the colums index to be extracted.
	MatRow SubMatrix(unsigned int idx, Container const & nbd_cols, MatRow const & M){
		if(*nbd_cols.crbegin() >= M.cols() || idx >= M.rows())
			throw std::runtime_error("Indeces exceed matrix dimension");
		MatRow res = indexing( M, Eigen::Map<const ArrInt> (&idx, 1), Eigen::Map<const ArrInt> (&(nbd_cols[0]), nbd_cols.size()) );
		return res;
	} //Passing cols to be exctracted and row index to be extracted.

	//------------------------------------------------------------------------------------------------------------------------------------------------------

	//Optimized Eigen operation. Takes matrix A of size (p+1 x p+1), exclude x-th row and x-th column and multiply the resulting matrix for vector b of size p.
	//It is optimized because matrix A is never copied and only the upper triangular part of the matrix is used. It is not needed to have a pass a symmetric 
	//matrix.
	//Remark -> correctness of parameters dimension is not checked.
	
	template<class MatType, Symmetric Sym = Symmetric::True>
	VecCol View_ExcMult(unsigned int const & x, MatType const & A, VecCol const & b)
	{
		static_assert(Sym == Symmetric::True || Sym == Symmetric::False, "Error, only possibilities for Symmetric template parameter are utils::Symmetric::True and Symmetric::False");
		unsigned int p = b.size();
		VecCol res(VecCol::Zero(p));
		if(x == 0){
			if constexpr(Sym == Symmetric::True)
			{
				res = A.bottomRightCorner(p,p). template selfadjointView<Eigen::Upper>() * b;
			}
			else
			{
				res = A.bottomRightCorner(p,p) * b;
			}
			return res;
		}
		else if(x == p){
			if constexpr(Sym == Symmetric::True)
			{
				res = A.topLeftCorner(p,p).template selfadjointView<Eigen::Upper>() * b;
			}
			else
			{
				res = A.topLeftCorner(p,p) * b;
			}
			
			return res;
		}
		else{
			if constexpr(Sym == Symmetric::True)
			{
				res.head(x)    = A.topLeftCorner(x,x).template selfadjointView<Eigen::Upper>() * b.head(x);
				res.head(x)   += A.block(0,x+1,x,p-x) * b.tail(p-x);
				res.tail(p-x)  = A.bottomRightCorner(p-x,p-x).template selfadjointView<Eigen::Upper>() * b.tail(p-x);
				res.tail(p-x) += A.block(0,x+1,x,p-x).transpose() * b.head(x);
			}
			else
			{
				res.head(x)    = A.topLeftCorner(x,x) * b.head(x);
				res.head(x)   += A.block(0,x+1,x,p-x) * b.tail(p-x);
				res.tail(p-x)  = A.bottomRightCorner(p-x,p-x) * b.tail(p-x);
				res.tail(p-x) += A.block(x+1,0,p-x,x) * b.head(x);
			}
			
			return res;
		}
		return res;
	}
	//------------------------------------------------------------------------------------------------------------------------------------------------------
		
	template<template <typename> class Graph, typename T = unsigned int >
	bool check_structure(Graph<T> const & G, MatRow const & data, double threshold = 1e-5){
		for(IdxType i = 1; i < data.rows()-1; ++i){
			for(IdxType j = i+1; j < data.cols(); ++j){
				if( ( G(i,j) == 0 && std::abs(data(i,j))>threshold ) || (G(i,j) == 1 && std::abs(data(i,j))<threshold)){
					//std::cout<<"Grafo("<<i<<", "<<j<<") = "<<G(i,j)<<"; Mat("<<i<<", "<<j<<") = "<<data(i,j)<<std::endl;
					return false;
				}
			}
		}
		return true;
	}
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	double power(double const & x, int const & esp){
		double res{1.0};
		if(esp == 0)
			return res;
		#pragma parallel for reduction(*:res)
		for(unsigned int i = 0; i < std::abs(esp); ++i)
			res *= x;
		if(esp > 0)
			return res;
		else
			return 1.0/res;
	} //Rarely used
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	
	// Taken from eigen manual https://eigen.tuxfamily.org/dox/TopicTemplateKeyword.html
	// For my needs it is enough to use that line without calling a function but it may be useful for more advanced stuff
	template <typename Derived1, typename Derived2>
	void copyUpperTriangularPart(Eigen::MatrixBase<Derived1>& dst, const Eigen::MatrixBase<Derived2>& src)
	{
	  dst.template triangularView<Eigen::Upper>() = src.template triangularView<Eigen::Upper>();
	}

	struct NormInf{
		using MatRow = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		static double norm(MatRow const & A, MatRow const & B){
			return (A - B).lpNorm<Eigen::Infinity>();
		}
	};
	struct Norm1{
		using MatRow = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		static double norm(MatRow const & A, MatRow const & B){
			return (A - B).template lpNorm<1>();
		}
	};
	struct NormSq{
		using MatRow = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		static double norm(MatRow const & A, MatRow const & B){
			return (A - B).squaredNorm();
		}
	};//NB, it returns the squared L2 norm ||A - B||^2
	struct MeanNorm{
		using MatRow = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		static double norm(MatRow const & A, MatRow const & B){
			Eigen::VectorXd Ones(Eigen::VectorXd::Constant(A.cols(), 1));
			return  ( static_cast<double>(Ones.transpose() * (A-B).cwiseAbs() * Ones) ) / static_cast<double>(A.cols()*A.cols());
		}
	}; //returns sum_ij( |a_ij - b_ij| )/N*N

	
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	enum class ScaleForm
	{
		Scale, InvScale, CholUpper_InvScale, CholLower_InvScale
	};
	//Omega_old, that is the matrix at the previous iteration is stored in the lower triangular part of Omega
	template<	template <typename> class GraphStructure = GraphType, typename T = unsigned int, 
				ScaleForm form = ScaleForm::InvScale, typename NormType = MeanNorm > //Templete parametes
	std::tuple< MatRow, bool, int>  //Return type
	rgwish_core( GraphStructure<T> const & G, double const & b, Eigen::MatrixXd & D, double const & threshold = 1e-8,
				 sample::GSL_RNG const & engine = sample::GSL_RNG(), unsigned int const & max_iter = 500 )
	{

		//Typedefs
		using Graph 	= GraphStructure<T>;
		using MatRow  	= Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		using MatCol   	= Eigen::MatrixXd;
		using IdxType  	= std::size_t;
		using iterator  = std::vector<unsigned int>::iterator;
		using citerator = std::vector<unsigned int>::const_iterator;
		//Checks
		static_assert(	internal_type_traits::isCompleteGraph<GraphStructure,T>::value,
						"___ERROR:_RGWISH_FUNCTION_REQUIRES_IN_INPUT_A_GRAPH_IN_COMPLETE_FORM. HINT -> EVERY_GRAPH_SHOULD_PROVIDE_A_METHOD_CALLED completeview() THAT_CONVERTS_IT_IN_THE_COMPLETE_FORM");				
		
		if(D.rows()!=D.cols())
			throw std::runtime_error("Non squared matrix inserted");
		if(G.get_size() != D.rows())
			throw std::runtime_error("Dimension of D is not equal to the number of nodes");
		//Set parameters
		unsigned int const N = G.get_size();
		unsigned int const n_links = G.get_n_links();
		bool converged = false;
		unsigned int it{0};
		double norm_res{1.0};

		sample::rchisq  rchisq;

		
		//if(n_links == 0) --> do nothing special, it is automatically handled

		//Step 1: Draw K from Wish(b,D) = wish(D^-1, b+N-1)
			//D matrix can be passed in different form. Usually D is an Inverse_scale parameter, however for sampling from Wishart distribution one need a Scale
			//matrix, i.e D^-1. The first step of every Wishart sampler is to perform a Cholesky decomposition of D^-1. This means that if this operation has to be
			//performed many times for the same D matrix, it is convinient to factorized it once and for all. Many choices are then possible and they influence
			//the function only in this particular point, in the sampling of matrix K. Once K is drawn, they all works the same way.
		MatCol K(MatCol::Zero(N,N));
		if constexpr(form == ScaleForm::Scale){ //D is D^-1, Scale matrix. 
			K = sample::rwish<MatCol, sample::isChol::False>()(engine, b, D);
		}
		else if constexpr(form == ScaleForm::InvScale){ //D is simply D. It has to be also inverted
			MatCol Inv_D(D.llt().solve(MatCol::Identity(N,N))); 
			K = sample::rwish<MatCol, sample::isChol::False>()(engine, b, Inv_D); 
		}
		else if constexpr(form == ScaleForm::CholUpper_InvScale){ // D is chol(D^-1) and it is upper triangular
			K = sample::rwish<MatCol, sample::isChol::Upper>()(engine, b, D);
		}
		else if constexpr(form == ScaleForm::CholLower_InvScale){ // D is chol(D^-1) and it is lower triangular
			K = sample::rwish<MatCol, sample::isChol::Lower>()(engine, b, D);
		}
		else{
			throw std::runtime_error("Error, rgwish needs to know what type of D matrix has been inserted. Possibilities are utils::ScaleForm::Scale, utils::ScaleForm::InvScale, utils::ScaleForm::CholUpper_InvScale, utils::ScaleForm::CholLower_InvScale");
		}
		
		if(n_links == G.get_possible_links()){
			//A complete Gwishart is a Wishart. Just return K in this case.
			return std::make_tuple(K, true, 0); 
		}
		//Step 2: Set Sigma=K^-1 and initialize Omega=Sigma
			MatRow Sigma(K.llt().solve(MatRow::Identity(N, N)));
			MatRow Omega(MatRow::Zero(Sigma.rows(),Sigma.cols()));
			Omega = Sigma;
		const std::map<unsigned int, std::vector<unsigned int> > nbd(G.get_nbd());
		//Start looping
		while(!converged && it < max_iter){
			it++;
			//For every node. 
			for(/*IdxType*/unsigned int i = 0; i < N; ++i){
			
				const std::vector<unsigned int>& nbd_i = nbd.find(i)->second;
				Eigen::VectorXd beta_i = Eigen::VectorXd::Zero(N-1);

				if(nbd_i.size() == 0){
					//do nothing
				}
				else if( nbd_i.size() == 1){
					const unsigned int &k = nbd_i[0];
					double beta_star_i = Sigma(k,i) / Omega(k,k); //In this case it is not a linear system but a simple, scalar, equation
					if(i == 0){
						beta_i.head(k) = Omega.block(1,k, k, 1) * beta_star_i;
						beta_i.tail(beta_i.size()-k) = Omega.block(k,k+1,1,beta_i.size()-k).transpose() * beta_star_i;
					}
					else if(i == N-1){
						beta_i.head(k) = Omega.block(0,k, k, 1) * beta_star_i;
						beta_i.tail(beta_i.size()-k) = Omega.block(k,k,1,beta_i.size()-k).transpose() * beta_star_i;
					}
					else{
						if(i < k){
							beta_i.head(i) = Omega.block(0,k, i, 1) * beta_star_i;
							beta_i.segment(i,k-i) = Omega.block(i+1,k, k-i, 1) * beta_star_i;
							beta_i.tail(beta_i.size()-k) = Omega.block(k,k+1,1,beta_i.size()-k).transpose() * beta_star_i;
						}
						else{ // k < i
							beta_i.head(k) = Omega.block(0,k,k,1) * beta_star_i;
							beta_i.segment(k,i-k) = Omega.block(k,k,1,i-k).transpose() * beta_star_i;
							beta_i.tail(beta_i.size()-i) = Omega.block(k,i+1,1,beta_i.size()-i).transpose() * beta_star_i;
						}
						//get k-th row except i-th column	
					}
				}
				else{
					//Step 3: Compute beta_star_i = (Omega_Ni_Ni)^-1*Sigma_Ni_i. beta_star_i in R^|Ni|
						MatRow Omega_Ni_Ni( SubMatrix<Symmetric::True>(nbd_i, Omega) );
						Eigen::VectorXd Sigma_Ni_i( SubMatrix(nbd_i, i, Sigma) ); 
						Eigen::VectorXd beta_star_i = Omega_Ni_Ni.llt().solve(Sigma_Ni_i);
					//Step 4: Define beta_hat_i in R^N-1 such that:
					//- Entries not associated to elements in Ni are 0
					//- Entries associated to elements in Ni are given by beta_star_i
					//- Has no element associated to i
						Eigen::VectorXd beta_hat_i(Eigen::VectorXd::Zero(N-1));
						for(citerator j_it = nbd_i.cbegin(); j_it < nbd_i.cend(); ++j_it){
							if(*j_it < i)
								beta_hat_i(*j_it) = beta_star_i( j_it - nbd_i.cbegin() );
							else if(*j_it > i)
								beta_hat_i(*j_it - 1) = beta_star_i( j_it - nbd_i.cbegin() );
						}
					//Step 5: Set i-th row and col of Omega equal to Omega_noti_noti*beta_hat_i. Omega_noti_noti is Omega without i-th row and i-th row
						beta_i = View_ExcMult(i, Omega, beta_hat_i);
				}
				//Plug beta_i into the i-th row / col except from diagonal element
				if(i == 0)//plug in first row (no first element)
					Omega.block(0,1,1,N-1) = beta_i.transpose();
				else if(i == N-1)//plug in last column (no last element)
					Omega.block(0,N-1,N-1,1) = beta_i;
				else{
					Omega.block(0,i,i,1) 	   			 = beta_i.head(i);
					Omega.block(i,i+1,1,beta_i.size()-i) = beta_i.transpose().tail(beta_i.size()-i);
				}
			}
		//Step 6: Compute the norm of differences
			norm_res = NormType::norm(Omega.template triangularView<Eigen::Upper>(), Omega.template triangularView<Eigen::Lower>().transpose());
			Omega.template triangularView<Eigen::Lower>() = Omega.template triangularView<Eigen::Upper>().transpose();
		//Step 7: Check stop criteria
			if(norm_res < threshold){
				converged = true;
			}
		}
		return std::make_tuple(Omega.template selfadjointView<Eigen::Upper>().llt().solve(MatRow::Identity(N, N)),converged, it);
	}
	

	//------------------------------------------------------------------------------------------------------------------------------------------------------
	//GraphStructure for the moment can only be GraphType / CompleteViewAdj / CompleteView
 	template<	template <typename> class GraphStructure = GraphType, typename T = unsigned int, 
				ScaleForm form = ScaleForm::InvScale, typename NormType = MeanNorm > //Templete parametes
	MatRow rgwish( GraphStructure<T> const & G, double const & b, Eigen::MatrixXd & D, double const & threshold = 1e-8,
				 	sample::GSL_RNG const & engine = sample::GSL_RNG(), unsigned int const & max_iter = 500 )
 	{
 		auto [Prec, conv, n_it] = rgwish_core<GraphStructure,T,form,NormType>(G,b,D,threshold,engine,max_iter);
 		return Prec;
 	}
 	
 	//------------------------------------------------------------------------------------------------------------------------------------------------------

 	//Thanks to rwish_core() function a lot of different possibilities are unified in one simple utility. Unfortunately it depends by many templete parameters.
 	//If running in c++ environment this is not a problem and it is a very compact solution. However in R everything is set run time and polymorphism is not allowed.
 	//The purpose of build_rgwish_function() is to select the correct type of call deciding runtime. The structure of the graph is still a templete parameter.
 	using rgwishRetType = std::tuple< MatRow, bool, int>;
 	template <template <typename> class GraphStructure, typename T>
 	using rgwish_function = std::function<rgwishRetType(GraphStructure<T> const &, double const &,Eigen::MatrixXd &, double const &, sample::GSL_RNG const &, unsigned int const &)>;
 	//Usage:
 	// auto rgwish_fun = utils::build_rgwish_function<CompleteView, unsigned int>(form, norm);
 	// auto rgwish_fun = utils::build_rgwish_function<GraphType, unsigned int>(form, norm);
 	// auto [Mat, converged, iter] = rgwish_fun(Graph.completeview(), b, D, threshold, engine, max_iter);
 	template<template <typename> class GraphStructure = GraphType, typename T = unsigned int>
 	rgwish_function<GraphStructure,T> build_rgwish_function(std::string const & form, std::string const & norm)
 	{
 		static_assert(	internal_type_traits::isCompleteGraph<GraphStructure, T>::value,
 						"___ERROR:_RGWISH_FUNCTION_REQUIRES_IN_INPUT_A_GRAPH_IN_COMPLETE_FORM. HINT -> EVERY_GRAPH_SHOULD_PROVIDE_A_METHOD_CALLED completeview() THAT_CONVERTS_IT_IN_THE_COMPLETE_FORM");				
 		if(form != "Scale" && form !="InvScale" && form != "CholLower_InvScale" && form != "CholUpper_InvScale")
 			throw std::runtime_error("Only possible forms are Scale, InvScale, CholLower_InvScale, CholUpper_InvScale");
 		if(norm == "Mean"){
 			if(form == "Scale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::Scale, MeanNorm>;
 			}
 			else if(form == "InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::InvScale, MeanNorm>;
 			}
 			else if(form == "CholLower_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholLower_InvScale, MeanNorm>;
 			}
 			else if(form == "CholUpper_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholUpper_InvScale, MeanNorm>;
 			}
 		}
 		else if(norm == "Inf"){
 			if(form == "Scale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::Scale, NormInf>;
 			}
 			else if(form == "InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::InvScale, NormInf>;
 			}
 			else if(form == "CholLower_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholLower_InvScale, NormInf>;
 			}
 			else if(form == "CholUpper_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholUpper_InvScale, NormInf>;
 			}

 		}
 		else if(norm == "One"){
 			if(form == "Scale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::Scale, Norm1>;
 			}
 			else if(form == "InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::InvScale, Norm1>;
 			}
 			else if(form == "CholLower_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholLower_InvScale, Norm1>;
 			}
 			else if(form == "CholUpper_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholUpper_InvScale, Norm1>;	
 			}

 		}
 		else if(norm == "Squared"){
 			if(form == "Scale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::Scale, NormSq>;
 			}
 			else if(form == "InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::InvScale, NormSq>;
 			}
 			else if(form == "CholLower_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholLower_InvScale, NormSq>;
 			}
 			else if(form == "CholUpper_InvScale"){
 				return rgwish_core<GraphStructure, T, ScaleForm::CholUpper_InvScale, NormSq>;	
 			}

 		}
 		else{ //error
 			throw std::runtime_error("The only available norms are Mean, Inf, One and Squared");
 		}
 		throw std::runtime_error("Error in building rgwish function");
 	}

	//------------------------------------------------------------------------------------------------------------------------------------------------------
	
	double logSumExp(double x, double y)
	{
		if(y > x)
			std::swap(x,y);
		return x + gsl_sf_log_1plusx(std::exp(y-x));
	}//Computes log(exp(x) + exp(y))
	double logSumExp(std::vector<double> const & v)
	{
		std::vector<double>::const_iterator it_max = std::max_element(v.cbegin(), v.cend());
		double res = std::accumulate(v.cbegin(), v.cend(), 0.0, [&it_max](double const & _res, double const & x){return _res + std::exp(x - *it_max);});
		return *it_max + std::log(res);
	}//Computes log(sum(exp(v_i)))
	double logSumExp(std::vector<double> const & v, double const & max)
	{
		double res = std::accumulate(v.cbegin(), v.cend(), 0.0, [&max](double const & _res, double const & x){return _res + std::exp(x - max);});
		return max + std::log(res);
	}//Computes log(sum(exp(v_i)))
	double log_mean(std::vector<double> const & v)
	{
		std::vector<double> log_v(v.size());
		std::transform (v.cbegin(), v.cend(), log_v.begin(), [](double const & x){
																if(x <= 0)
																	throw std::runtime_error("log_mean requires all the elements to be positive.");
																else
																	return std::log(x);}
						);
		return -std::log(v.size()) + logSumExp(log_v);
	}//Computes log( mean(v) )

	//------------------------------------------------------------------------------------------------------------------------------------------------------
	//GraphStructure right now can only be GraphType / CompleteViewAdj / CompleteView
	template<template <typename> class GraphStructure = GraphType, typename Type = unsigned int >
	long double log_normalizing_constat(GraphStructure<Type> const & G, double const & b, Eigen::MatrixXd const & D, unsigned int const & MCiteration, 
										sample::GSL_RNG const & engine = sample::GSL_RNG())
	{
		//Typedefs
		using Graph 	  = GraphStructure<Type>;
		using MatRow  	  = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		using MatCol      = Eigen::MatrixXd;
		using CholTypeCol = Eigen::LLT<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>, Eigen::Lower>;
		using iterator    = std::vector<unsigned int>::iterator;
		using citerator   = std::vector<unsigned int>::const_iterator;
		//Check
		static_assert(	internal_type_traits::isCompleteGraph<GraphStructure, Type>::value,
						"___ERROR:_lOG_NORMALIZING_CONSTANT_FUNCTION_REQUIRES_IN_INPUT_A_GRAPH_IN_COMPLETE_FORM. HINT -> EVERY_GRAPH_SHOULD_PROVIDE_A_METHOD_CALLED completeview() THAT_CONVERTS_IT_IN_THE_COMPLETE_FORM");				
		if(b <= 2)
			throw std::runtime_error("Shape parameter has to be larger than 2");
		if(D != D.transpose()){
			throw std::runtime_error("Inv_Scale matrix is not symetric");
		}
		CholTypeCol cholD(D);
		if( cholD.info() != Eigen::Success)
			throw std::runtime_error("Chol decomposition of Inv Scale matrix failed, probably the matrix is not sdp");
		//Step 1: Preliminaries
		const unsigned int n_links(G.get_n_links());
		const unsigned int N(G.get_size());
		const unsigned int max_n_links(0.5*N*(N-1));
		const unsigned int N_free_elements(n_links + N);
		unsigned int number_nan{0};
		long double result_MC{0};
		sample::rnorm rnorm;
		sample::rchisq rchisq;
		//Start
		//nu[i] = #1's in i-th row, from position i+1 up to end. Note that it is also  che number of off-diagonal elements in each row
		auto start = std::chrono::high_resolution_clock::now();
		std::vector<unsigned int> nu(N);
		#pragma omp parallel for shared(nu)
		for(IdxType i = 0; i < N; ++i){
			std::vector<unsigned int> nbd_i = G.get_nbd(i);
			nu[i] = std::count_if(nbd_i.cbegin(), nbd_i.cend(), [i](const unsigned int & idx){return idx > i;});
		}
		if(n_links == max_n_links){
			//Complete graph
			long double res_gamma{0};
			for(IdxType i = 0; i < N; ++i){
				res_gamma += std::lgammal( (long double)(0.5*(b + nu[i])) );
			}
			return ( 	(N/2)*utils::log_pi +
						(0.5*N*(b+N-1))*utils::log_2 +
		        	  	res_gamma -
		        	 	0.5*(b+N-1)*std::log(D.determinant())   );
		}
		else if(n_links == 0){
			//Empty graph
			long double sum_log_diag{0};
			for(IdxType i = 0; i < N; ++i){
				sum_log_diag += std::log( D(i,i) );
			}
			return ( 	0.5*N*b*utils::log_2 +
						N*std::lgamma(0.5*b) -
						(0.5*b) *sum_log_diag 	);
		}
		else{
			//- Compute T = chol(D^-1), T has to be upper diagonal
			MatCol T(cholD.solve(Eigen::MatrixXd::Identity(N,N)).llt().matrixU()); //T is colwise because i need to extract its columns
			//- Define H st h_ij = t_ij/t_jj
			MatCol H(MatCol::Zero(N,N)); //H is colwise because i would need to scan its col
			for(unsigned int j = 0; j < N ; ++j)
				H.col(j) = T.col(j) / T(j,j);
			std::vector<double> vec_ss_nonfree;
			std::vector<double> vec_ss_nonfree_result; //useful for parallel case, does not harm in sequential case (no unuseful copies are done)
			#pragma omp parallel private(vec_ss_nonfree),shared(vec_ss_nonfree_result)
			{
				int n_threads{1};
				#ifdef PARALLELEXEC
					n_threads = omp_get_num_threads();
				#endif
				vec_ss_nonfree.reserve(MCiteration/n_threads);
				//Start MC for loop
				for(IdxType iter = 0; iter < MCiteration/n_threads; ++ iter){
					MatRow Psi(MatRow::Zero(N,N));
					double sq_sum_nonfree{0}; //In the end it has to be exp(-1/2 sum( psi_nonfree_ij^2 )). I accumulate the sum of non free elements squared every time they are generated
					//Step 2: Sampling the free elements
					//- Sample the diagonal elements from chisq(b + nu_i) (or equivalently from a gamma((b+nu_i)/2, 1/2))
					//- Sample the off-diagonal elements from N(0,1)

					std::vector<double> vector_free_element(N_free_elements);
					unsigned int time_to_diagonal{0};
					citerator it_nu = nu.cbegin();
					for(unsigned int i = 0; i < N_free_elements; ++i){
						if(time_to_diagonal == 0){
							vector_free_element[i] = std::sqrt(rchisq(engine, (double)(b+(*it_nu)) ));
						}
						else
							vector_free_element[i] = rnorm(engine);

						if(time_to_diagonal++ == *it_nu){
							time_to_diagonal = 0;
							it_nu++;
						}
					}

					std::vector<double>::const_iterator it_fe = vector_free_element.cbegin();

					if(H.isIdentity()){ //Takes into account also the case D diagonal 
						//Step 3: Complete Psi (upper part)
						//- If i==0 -> all null
						Psi(0,0) = *it_fe;
						it_fe++;
						for(unsigned int j = 1; j < N; ++j){
							if(G(0,j) == true){
								Psi(0,j) = *it_fe;
								it_fe++;
							}
						}

						Psi(1,1) = *it_fe; //Counting also the diagonal element because they have to be updated too
						it_fe++;
						for(unsigned int j = 2; j < N; ++j){ 
							if(G(1,j) == false){
								Psi(1,j) = - Psi(0,1)*Psi(0,j)/Psi(1,1);
								sq_sum_nonfree += Psi(1,j)*Psi(1,j);
							}
							else{
								Psi(1,j) = *it_fe;
								it_fe++;
							}
						}

						//- If i>1 -> formula 2
						for(unsigned int i = 2; i < N-1; ++i){ // i = N-1 (last element) is useless
							Eigen::VectorXd S = Psi.block(0,i,i,1); //Non cache friendly 
							for(unsigned int j = i; j < N; ++j){ //Counting also the diagonal element because they have to be updated too
								if(G(i,j) == false){
									Eigen::VectorXd S_star = Psi.block(0,j,i,1);
									Psi(i,j) = - S.dot(S_star)/Psi(i,i) ;
									sq_sum_nonfree += Psi(i,j)*Psi(i,j);
								}
								else{
									Psi(i,j) = *it_fe;
									it_fe++;
								}
							}
						}

					}
					else{
						auto CumSum = [&Psi, &H](unsigned int const & a, unsigned int const & b){ //Computes sum_(k in a:b-1)(Psi_ak*H_kb)

							if(a >= Psi.rows() || b >= H.rows())
								throw std::runtime_error("Cum Sum invalid index request");
							if(a>=b)
								throw std::runtime_error("Wrong dimension inserted");
							else if(a == (b-1))
								return Psi(a,b-1)*H(a,b);
							else
								return (Eigen::RowVectorXd(Psi.block(a,a,1,b-a)).dot(Eigen::VectorXd(H.block(a,b,b-a,1)))); //Cache friendly
						};
						//Step 3: Complete Psi (upper part)
						//- If i==0 -> formula 1
						// Sums is a NxN-1 matrix such that Sums(a,b) = CumSum(a,b+1), i.e CumSum(a,b) = Sums(a,b-1)
						Psi(0,0) = *it_fe;
						it_fe++;
						MatRow Sums(MatRow::Zero(N,N-1));
						for(unsigned int j = 1; j < N; ++j){
							Sums(0,j-1) = CumSum(0,j);
							if(G(0,j) == false){
								Psi(0,j) = -Sums(0,j-1);
								sq_sum_nonfree += Psi(0,j)*Psi(0,j);
							}
							else{
								Psi(0,j) = *it_fe;
								it_fe++;
							}
						}
						//- If i==1 -> simplified case of formula 2
						Psi(1,1) = *it_fe;
						it_fe++;
						for(unsigned int j = 2; j < N; ++j){
							Sums(1,j-1) = CumSum(1,j);
							if(G(1,j) == false){
								Psi(1,j) = - ( Sums(1,j-1) + (Psi(0,1) + Psi(0,0)*H(0,1))*(Psi(0,j) + Sums(0,j-1))/Psi(1,1) );
								sq_sum_nonfree += Psi(1,j)*Psi(1,j);
							}
							else{
								Psi(1,j) = *it_fe;
								it_fe++;
							}
						}
						//- If i>1 -> formula 2
						for(unsigned int i = 2; i < N-1; ++i){
							Psi(i,i) = *it_fe;
							it_fe++;
							Eigen::VectorXd S = Psi.block(0,i,i,1) + Sums.block(0,i-1,i,1); //non cache friendly 
							for(unsigned int j = i+1; j < N; ++j){
								Sums(i,j-1) = CumSum(i,j);
								if(G(i,j) == false){
									Eigen::VectorXd S_star = Psi.block(0,j,i,1) + Sums.block(0,j-1,i,1); //non cache friendly 
									Psi(i,j) = - ( Sums(i,j-1) + S.dot(S_star)/Psi(i,i) );
									sq_sum_nonfree += Psi(i,j)*Psi(i,j);
								}
								else{
									Psi(i,j) = *it_fe;
									it_fe++;
								}
							}
						}
					}
					//Step 4: Compute exp( -0.5 * sum_NonFreeElements(Psi_ij)^2 )
					if( sq_sum_nonfree != sq_sum_nonfree){
						number_nan++;
					}
					else{
						vec_ss_nonfree.emplace_back(-0.5*sq_sum_nonfree);
					}
				}
				#ifdef PARALLELEXEC
					#pragma omp barrier
					#pragma omp critical
					{
						vec_ss_nonfree_result.insert(vec_ss_nonfree_result.end(), 
													 std::make_move_iterator(vec_ss_nonfree.begin()), std::make_move_iterator(vec_ss_nonfree.end())
													);
					}	
				#endif
				
			}
			#ifndef PARALLELEXEC
				std::swap(vec_ss_nonfree_result, vec_ss_nonfree);
			#endif
			result_MC = -std::log(vec_ss_nonfree_result.size()) + logSumExp(vec_ss_nonfree_result);
			//Step 5: Compute constant term and return
			long double result_const_term{0};
			#pragma parallel for reduction(+:result_const_term)
			for(IdxType i = 0; i < N; ++i){
				result_const_term += (long double)nu[i]/2.0*log_2pi +
									 (long double)(b+nu[i])/2.0*log_2 +
									 (long double)(b+G.get_nbd(i).size())*std::log(T(i,i)) + 
									 std::lgammal((long double)(0.5*(b + nu[i])));
			}//This computation requires the best possible precision because il will generate a very large number
			return result_MC + result_const_term;
		}
	}
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	
	//Function for extracting the upper triangular part of a RowMajor matrix. It is required because Eigen::TriangularView still stores the lower part, it
	//is simply never used. This version is then more memory friendly.
	VecCol get_upper_part(MatRow const & Mat)
	{
		if(Mat.rows() != Mat.cols())
			throw std::runtime_error("A squared matrix is needed as input");
		unsigned int N = Mat.rows();
		VecCol res(VecCol::Zero(0.5*N*(N-1) + N)); //dimension is equal to all extra-diagonal terms plus diagonal
		int start_pos{0};
		unsigned int n_elem{N};
		for(unsigned int i = 0; i < Mat.rows(); ++i){
			res.segment(start_pos, n_elem) = Mat.block(i,i,1,n_elem).transpose();
			start_pos += n_elem;
			n_elem--;
		}
		return res;
	}

	//------------------------------------------------------------------------------------------------------------------------------------------------------

	//Not used in R package, data are simulated in R in that case. Those are only to test c++ functions
	std::tuple<MatCol, MatCol, VecCol, double, MatRow, std::vector<bool> > 
	SimulateData_Block(unsigned int const & p, unsigned int const & n, unsigned int const & r,
					   MatCol const & BaseMat, BlockGraph<bool> & G,  unsigned int seed = 0)
	{
	
		sample::GSL_RNG engine(seed);
		sample::rmvnorm rmv; //Covariance parametrization
		MatRow Ip(MatRow::Identity(p,p));
		MatRow Ir(MatRow::Identity(r,r));

		//Precision
		MatCol Icol(MatCol::Identity(p,p));
		MatRow K = utils::rgwish(G.completeview(), 3.0, Icol, 1e-14,engine,500);
		//mu
		VecCol mu(VecCol::Zero(p));
		for(unsigned int i = 0; i < p; ++i)
			mu(i) = sample::rnorm()(engine, 0, 0.1);
		//tau_eps
		double tau_eps = sample::rnorm()(engine, 100, 0.1);
		//Beta and data
		MatCol Beta(MatCol::Zero(p,n));
		MatCol data(MatCol::Zero(r,n));
		MatRow Sigma = K.inverse();
		MatRow Cov_tau = (1/tau_eps)*Ir;
		for(unsigned int i = 0; i < n; ++i){
			Beta.col(i) = rmv(engine, mu, Sigma);
			VecCol media(BaseMat*Beta.col(i));
			data.col(i) = rmv(engine, media, Cov_tau);
		}
		return std::make_tuple(data, Beta, mu, tau_eps, K, G.get_adj_list());
	}
	std::tuple<MatCol, MatCol, VecCol, double, MatRow, std::vector<bool> > 
	SimulateData_Block(unsigned int const & p, unsigned int const & n, unsigned int const & r,
					   MatCol const & BaseMat, std::shared_ptr<const Groups> const & Gr,  unsigned int seed = 0, double const & sparsity = 0.3)
	{
		//Graph
		BlockGraph<bool> G(Gr);
		G.fillRandom(sparsity, seed);
		return SimulateData_Block(p,n,r,BaseMat,G,seed);
	}
	std::tuple<MatCol, MatCol, VecCol, double, MatRow, std::vector<bool> > 
	SimulateData_Block(unsigned int const & p, unsigned int const & n, unsigned int const & r, unsigned int const & M, 
					   MatCol const & BaseMat, unsigned int seed = 0, double const & sparsity = 0.3)
	{
		return SimulateData_Block(p, n, r, BaseMat, std::make_shared<const Groups>(M,p), seed, sparsity);
	}

	std::tuple<MatCol, MatCol, VecCol, double, MatRow, std::vector<bool> > 
	SimulateData_Complete(unsigned int const & p, unsigned int const & n, unsigned int const & r,
					  	  MatCol const & BaseMat, GraphType<bool> & G, unsigned int seed = 0)
	{

		sample::GSL_RNG engine(seed);
		sample::rmvnorm rmv; //Covariance parametrization
		MatRow Ip(MatRow::Identity(p,p));
		MatCol Ip_col(MatCol::Identity(p,p));
		MatRow Ir(MatRow::Identity(r,r));
		
		//Precision
		MatRow K = utils::rgwish(G.completeview(), 3.0, Ip_col, 1e-14,engine);
		//mu
		VecCol mu(VecCol::Zero(p));
		//for(unsigned int i = 0; i < p; ++i)
			//mu(i) = sample::rnorm()(engine, 0, 0.1);
				//std::cout<<"mu:"<<std::endl<<mu<<std::endl;
		//tau_eps
		double tau_eps = sample::rnorm()(engine, 100, 0.1);
				//std::cout<<"tau_eps = "<<tau_eps<<std::endl;
		//Beta and data
		MatCol Beta(MatCol::Zero(p,n));
		MatCol data(MatCol::Zero(r,n));
		MatRow Sigma = K.inverse();
		MatRow Cov_tau = (1/tau_eps)*Ir;
		for(unsigned int i = 0; i < n; ++i){
			Beta.col(i) = rmv(engine, mu, Sigma);
			VecCol media(BaseMat*Beta.col(i));
			data.col(i) = rmv(engine, media, Cov_tau);
		}

		
		return std::make_tuple(data, Beta, mu, tau_eps, K, G.get_adj_list());
	}

	std::tuple<MatCol, MatCol, VecCol, double, MatRow, std::vector<bool> > 
	SimulateData_Complete(unsigned int const & p, unsigned int const & n, unsigned int const & r,
					  	  MatCol const & BaseMat, unsigned int seed = 0, double const & sparsity = 0.3)
	{

		GraphType<bool> G(p);
		G.fillRandom(sparsity, seed);
		
		return SimulateData_Complete(p,n,r,BaseMat,G,seed);
	}


	//------------------------------------------------------------------------------------------------------------------------------------------------------

	using MatRow  	= Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
	using MatCol   	= Eigen::MatrixXd;
	using VecCol    = Eigen::VectorXd;
	//Those functions all return:
	// - A pxp matrix representing sum_i=1:n[(beta_i - mu)*(beta_i - mu)^T]
	// - A precision matrix
	// - The adj list of a graph
	std::tuple<MatCol, MatRow, std::vector<bool> > 
	SimulateDataGGM_Block(unsigned int const & p, unsigned int const & n, BlockGraph<bool> & G, unsigned int seed = 0, bool mean_null = true)
	{
	
		sample::GSL_RNG engine(seed);
		sample::rmvnorm_prec<sample::isChol::False> rmv; //Precision parametrization
		//Precision
		MatCol Icol(MatCol::Identity(p,p));
		MatRow K = utils::rgwish(G.completeview(), 3.0, Icol,1e-14,engine);
		//mu
		VecCol mu(VecCol::Zero(p));
		if(!mean_null){
			for(unsigned int i = 0; i < p; ++i)
				mu(i) = sample::rnorm()(engine, 0, 0.1);
		}
		//Beta and data
		MatCol data(MatCol::Zero(p,p));
		for(unsigned int i = 0; i < n; ++i){
			VecCol beta_i = rmv(engine, mu, K);
			data += (beta_i - mu)*(beta_i - mu).transpose();
		}
		
		return std::make_tuple(data, K, G.get_adj_list());
	}
	std::tuple<MatCol, MatRow, std::vector<bool> > 
	SimulateDataGGM_Block(	unsigned int const & p, unsigned int const & n, std::shared_ptr<const Groups> const & Gr,  unsigned int seed = 0, 
							bool mean_null = true, double const & sparsity = 0.3)
	{
		//Graph
		BlockGraph<bool> G(Gr);
		G.fillRandom(sparsity, seed);
		return SimulateDataGGM_Block(p, n, G, seed, mean_null);
	}
	std::tuple<MatCol, MatRow, std::vector<bool> > 
	SimulateDataGGM_Block(unsigned int const & p, unsigned int const & n, unsigned int const & M, unsigned int seed = 0, bool mean_null = true, double const & sparsity = 0.3)
	{
		return SimulateDataGGM_Block(p, n, std::make_shared<const Groups>(M,p), seed, mean_null, sparsity);
	}

	std::tuple<MatCol, MatRow, std::vector<bool> > 
	SimulateDataGGM_Complete(unsigned int const & p, unsigned int const & n, GraphType<bool> & G, unsigned int seed = 0, bool mean_null = true)
	{

		sample::GSL_RNG engine(seed);
		sample::rmvnorm_prec<sample::isChol::False> rmv; //Precision parametrization
		//Precision
		MatCol Icol(MatCol::Identity(p,p));
		MatRow K = utils::rgwish(G, 3.0, Icol, 1e-14, engine);
		//mu
		VecCol mu(VecCol::Zero(p));
		if(!mean_null){
			for(unsigned int i = 0; i < p; ++i)
				mu(i) = sample::rnorm()(engine, 0, 0.1);
		}

		//Beta and data
		MatCol data(MatCol::Zero(p,p));
		for(unsigned int i = 0; i < n; ++i){
			VecCol beta_i = rmv(engine, mu, K);
			data += (beta_i - mu)*(beta_i - mu).transpose();
		}
		return std::make_tuple(data, K, G.get_adj_list());
	}
	std::tuple<MatCol, MatRow, std::vector<bool> > 
	SimulateDataGGM_Complete(unsigned int const & p, unsigned int const & n, unsigned int seed = 0, bool mean_null = true, double const & sparsity = 0.3)
	{
		//Graph
		GraphType<bool> G(p);
		G.fillRandom(sparsity, seed);
		return SimulateDataGGM_Complete(p, n, G, seed, mean_null);
	}
	//------------------------------------------------------------------------------------------------------------------------------------------------------
	

	//list_all_graphs() function creates all possible graphs having a p nodes. Block form is also allowed. For the moment, it is not exported in R.
	//As p grows, this function is inevitably very very slow, too many graphs have to be created. Use it only for n_el < 20

	//Usage:
	//std::vector< GraphType<unsigned int> > all_G_c = utils::list_all_graphs(p);
	//std::vector< BlockGraph<bool> > all_G_b = utils::list_all_graphs<BlockGraph, bool>(ptr_gruppi_nosin);
	//Template parameters are not deduced. They have to be specified  or defaulted

	template< class T = unsigned int>
	void build_adjs(std::vector< std::vector<T> > & all_G, std::vector<T> g, unsigned int const & n_el){
		if(g.size() == n_el)
			all_G.emplace_back(g);
		else{
			std::vector<T> g_left(g);
			std::vector<T> g_right(g);
			g_left.emplace_back(true);
			g_right.emplace_back(false);
			build_adjs(all_G, g_left, n_el);
			build_adjs(all_G, g_right, n_el);
		}
	}

	template<template <typename> class GraphStructure = GraphType, typename T = unsigned int >
	std::vector< GraphStructure<T> > list_all_graphs(unsigned int p = 0, std::shared_ptr<const Groups> const & ptr_groups = nullptr, bool print = false){
		using Graph = GraphStructure<T>;
			
		if( p <= 0 && ptr_groups == nullptr )
			throw std::runtime_error("Wrong dimension inserted, need to know the dimension of the Graph or the list of Groups");
		unsigned int n_el{0};
		if constexpr(internal_type_traits::isCompleteGraph<GraphStructure, T>::value){
			n_el = 0.5*(p*p - p);
		}
		else if constexpr( internal_type_traits::isBlockGraph<GraphStructure, T>::value /*std::is_same_v<Graph, BlockGraph<T> > || std::is_same_v<Graph, BlockGraphAdj<T> >*/){
			if(ptr_groups == nullptr)
				throw std::runtime_error("In case of block graphs, it is mandatory to pass the list of Groups");	
			n_el = ptr_groups->get_possible_block_links();
		}
		else
			throw std::runtime_error("The inserted graph is not Complete nor Block. Library is well tested is types are GraphType, BlockGraph or BlockGraphAdj. If a new type of graph was implemented, make sure that internal_type_traits.h has been correctly updated.");
		if(n_el > 20)
		 	std::cout<<"Very large graph required: "<<n_el<<" possible links and "<<utils::power(2.0, n_el)<<" possible graphs"<<std::endl;
	   	std::vector< std::vector<T> > all_G; 
	   	std::vector<T> g;
	   	build_adjs<T>(all_G, g, n_el);

	   	if(print){
	   		for(int i = 0; i < all_G.size(); ++i){
	   			std::cout<<"["<<i<<"] -> ";
	   			for(auto __v : all_G[i])
	   				std::cout<<__v<<", ";
	   		std::cout<<std::endl;
	   	}

	   	}
	   
	   	std::vector< Graph > result;
	   	result.reserve(all_G.size());
	   	
	   	if constexpr(internal_type_traits::isCompleteGraph<GraphStructure, T>::value){
			for(int i = 0; i < all_G.size(); ++i){
				result.emplace_back(Graph (all_G[i]) );
			}
		}
		else if constexpr(internal_type_traits::isBlockGraph<GraphStructure, T>::value){
			for(int i = 0; i < all_G.size(); ++i){
				result.emplace_back(Graph (all_G[i], ptr_groups) );
			}
		}
		
		return result;
	}

	template<template <typename> class GraphStructure = BlockGraph, typename T = unsigned int >
	std::vector< GraphStructure<T> > list_all_graphs(std::shared_ptr<const Groups> const & ptr_groups, bool print = false){
		using Graph = GraphStructure<T>;
		static_assert(	internal_type_traits::isBlockGraph<GraphStructure, T>::value ,
						"Wrong type of graph inserted. The specialization for block graphs has been called, this means that only Block graphs are allowed." );
		return list_all_graphs<GraphStructure, T>(0,ptr_groups, print);
	}







}


#endif

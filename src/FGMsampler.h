#ifndef __GSBASE_HPP__
#define __GSBASE_HPP__

#include "SamplerOptions.h"

/*FGM sampler for sampling from a functional graphical model. It both perform a smoothing procedure and the estimation of the graph describing the dependece structure of regression coefficients*/

template<template <typename> class GraphStructure = GraphType, typename T = unsigned int  >
class FGMsampler : public SamplerTraits
{
	public:
	using Graph 	    = GraphStructure<T>;
	using CompleteType  = typename GGMTraits<GraphStructure, T>::CompleteType;
	using PrecisionType = typename GGMTraits<GraphStructure, T>::PrecisionType;
	using GGMType 	    = std::unique_ptr<GGM<GraphStructure, T>>;
	//using RetType 	    = std::tuple<RetBeta, RetMu, RetK, RetGraph, RetTaueps>;

	FGMsampler( MatCol const & _data, Parameters const & _params, Hyperparameters const & _hy_params, 
			    Init<GraphStructure, T> const & _init, GGMType & _GGM_method, std::string const & _file_name = "FGMresult", unsigned int _seed = 0, bool _print_pb = true ):
			    data(_data), params(_params), hy_params(_hy_params), ptr_GGM_method(std::move(_GGM_method)) ,init(_init),
				p(_init.Beta0.rows()), n(_init.Beta0.cols()), grid_pts(_params.Basemat.rows()), engine(_seed), print_pb(_print_pb), file_name(_file_name)
	{
	 	this->check();
	 	file_name += ".h5";
	} 

	int run();

	private:
	void check() const;
	MatCol data; //grid_pts x n
	Parameters params;
	Hyperparameters hy_params;
	GGMType ptr_GGM_method;
	Init<GraphStructure, T> init;
	unsigned int p;
	unsigned int n;
	unsigned int grid_pts;
	sample::GSL_RNG engine;
	int total_accepted{0};
	int visited{0};
	bool print_pb;
	std::string file_name;
};


template<template <typename> class GraphStructure, typename T >
void FGMsampler<GraphStructure, T >::check() const{
	if(data.rows() != grid_pts) //check for grid_pts
		throw std::runtime_error("Error, incoerent number of grid points");
	if(data.cols() != n) //check for n
		throw std::runtime_error("Error, incoerent number of data");
	if( ptr_GGM_method->get_inv_scale().cols() != p || params.Basemat.cols() != p || 
		p != init.mu0.size() || p != init.K0.cols() || init.K0.cols() != init.K0.rows() || 
		p != init.G0.get_complete_size()) //check for p
			throw std::runtime_error("Error, incoerent number of basis");
}


template<template <typename> class GraphStructure, typename T  >
int FGMsampler<GraphStructure, T >::run()
{
	
	//Typedefs
	GGM<GraphStructure, T> & GGM_method= *ptr_GGM_method;

	// Declare all parameters (makes use of C++17 structured bindings)
	const unsigned int & r = grid_pts;
	const double&  a_tau_eps = this->hy_params.a_tau_eps;
	const double&  b_tau_eps = this->hy_params.b_tau_eps;
	const double&  sigma_mu  = this->hy_params.sigma_mu;
	const double&  p_addrm   = this->hy_params.p_addrm; 
	const auto &[niter, nburn, thin, thinG, MCiterPrior, MCiterPost,Basemat, ptr_groups, iter_to_store, iter_to_storeG, threshold] = this->params;
	MatCol Beta = init.Beta0; //p x n
	VecCol mu = init.mu0; // p
	double tau_eps = init.tau_eps0; //scalar
	MatRow K = init.K0; 
	Graph  G = init.G0;
	GGM_method.init_precision(G,K); 
	total_accepted = 0;
	visited = 0;
	const unsigned int prec_elem = 0.5*p*(p+1); //Number of elements in the upper part of precision matrix (diagonal inclused). It is what is saved of the Precision matrix
	unsigned int n_graph_elem{0};
	if constexpr( internal_type_traits::isBlockGraph<GraphStructure, T>::value ){
		n_graph_elem = G.get_possible_block_links();
	}
	else{
		n_graph_elem = G.get_possible_links();
	}
	unsigned int it_saved{0};
	unsigned int it_savedG{0};
	
	//Random engine and distributions
	sample::rmvnorm rmv; //Covariance parametrization
	sample::rgamma  rGamma;

	//Define all those quantities that can be compute once
	const MatRow tbase_base = Basemat.transpose()*Basemat; // p x p
	const MatCol tbase_data = Basemat.transpose()*data;	 //  p x n
	const double Sdata(data.cwiseProduct(data).sum()); // Sum_i(<yi, yi>) sum of the inner products of each data 
	const double Sdata_btaueps(Sdata+b_tau_eps);
	const double a_tau_eps_post = (n*r + a_tau_eps)*0.5;	
	const MatRow Irow(MatRow::Identity(p,p));
	const MatRow one_over_sigma_mu((1/sigma_mu)*Irow);
									

	//Open file
	HDF5conversion::FileType file;
	file = H5Fcreate(file_name.data(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if(file < 0)
		throw std::runtime_error("Cannot create the file. The most probable reason is that the execution was stopped before closing a file having the same name of the one that was asked to be generated. Delete the old file or change the name of the new one");

	int bi_dim_rank = 2; //for 2-dim datasets. Beta are matrices
	int one_dim_rank = 1;//for 1-dim datasets. All other quantities
	//Print info file
	HDF5conversion::DataspaceType dataspace_info;
	HDF5conversion::ScalarType Dim_info = 4;
	dataspace_info = H5Screate_simple(one_dim_rank, &Dim_info, NULL);
	HDF5conversion::DatasetType  dataset_info;
	dataset_info  = H5Dcreate(file,"/Info", H5T_NATIVE_UINT, dataspace_info, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_info < 0)
		throw std::runtime_error("Error, can not create dataset for Info");
	{
		std::vector< unsigned int > info{p,n,iter_to_store,iter_to_storeG};
		unsigned int * buffer_info = info.data();		
		HDF5conversion::StatusType status = H5Dwrite(dataset_info, H5T_NATIVE_UINT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer_info);
	}
	//Print what sampler has been used
	HDF5conversion::DatasetType  dataset_version;
	HDF5conversion::DataspaceType dataspace_version = H5Screate(H5S_NULL);
	dataset_version = H5Dcreate(file, "/Sampler", H5T_STD_I32LE, dataspace_version, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_version < 0)
		throw std::runtime_error("Error, can not create dataset for Sampler");
	HDF5conversion::WriteString(dataset_version, "FGMsampler");
	//Create dataspaces
	HDF5conversion::DataspaceType dataspace_Beta, dataspace_Mu, dataspace_Prec, dataspace_TauEps, dataspace_Graph;
	HDF5conversion::ScalarType Dim_beta_ds[2] = {p, n*iter_to_store}; 
	HDF5conversion::ScalarType Dim_mu_ds = p*iter_to_store;
	HDF5conversion::ScalarType Dim_K_ds = prec_elem*iter_to_storeG;
	HDF5conversion::ScalarType Dim_G_ds = n_graph_elem*iter_to_storeG;
	HDF5conversion::ScalarType Dim_taueps_ds = iter_to_store;

	dataspace_Beta   = H5Screate_simple(bi_dim_rank,   Dim_beta_ds,   NULL);
	dataspace_Mu     = H5Screate_simple(one_dim_rank, &Dim_mu_ds,     NULL);
	dataspace_TauEps = H5Screate_simple(one_dim_rank, &Dim_taueps_ds, NULL);
	dataspace_Prec   = H5Screate_simple(one_dim_rank, &Dim_K_ds,      NULL);
	dataspace_Graph  = H5Screate_simple(one_dim_rank, &Dim_G_ds,      NULL);

	//Create dataset
	HDF5conversion::DatasetType  dataset_Beta, dataset_Mu, dataset_Prec, dataset_TauEps, dataset_Graph;
	dataset_Beta  = H5Dcreate(file,"/Beta", H5T_NATIVE_DOUBLE, dataspace_Beta, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_Beta < 0)
		throw std::runtime_error("Error, can not create dataset for Beta");
	dataset_Mu = H5Dcreate(file,"/Mu", H5T_NATIVE_DOUBLE, dataspace_Mu, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_Mu < 0)
		throw std::runtime_error("Error, can not create dataset for Mu");
	dataset_TauEps  = H5Dcreate(file,"/TauEps", H5T_NATIVE_DOUBLE, dataspace_TauEps, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_TauEps < 0)
		throw std::runtime_error("Error, can not create dataset for TauEps");
	dataset_Prec = H5Dcreate(file,"/Precision", H5T_NATIVE_DOUBLE, dataspace_Prec, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_Prec < 0)
		throw std::runtime_error("Error, can not create dataset for Precision");
	dataset_Graph = H5Dcreate(file,"/Graphs", H5T_NATIVE_UINT, dataspace_Graph, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if(dataset_Graph < 0)
		throw std::runtime_error("Error, can not create dataset for Graphs");

	//Setup for progress bar, need to specify the total number of iterations
	pBar bar(niter);
	 
	//Start MCMC loop
	for(int iter = 0; iter < niter; iter++){
		
		//Show progress bar
		bar.update(1);
		if(print_pb){
			bar.print(); 	
		}
		int accepted_mv{0};
		//mu
		VecCol S_beta(Beta.rowwise().sum());
		CholTypeRow chol_invA(one_over_sigma_mu + n*K); 
		MatRow A(chol_invA.solve(Irow));
		VecCol a(A*(K*S_beta));
		mu = rmv(engine, a, A);


		//Beta
		CholTypeRow chol_invBn(tau_eps*tbase_base + K); 
		MatRow Bn(chol_invBn.solve(Irow));
		VecCol Kmu(K*mu);
		//Quantities needed down the road
		MatCol U(MatCol::Zero(p,p)); //Has to be ColMajor
		double b_tau_eps_post(Sdata_btaueps);
		#pragma omp parallel for shared(Beta, U), reduction(+ : b_tau_eps_post)
		for(unsigned int i = 0; i < n; ++i){
			VecCol bn_i = Bn*(tau_eps*tbase_data.col(i) + Kmu); 
			VecCol beta_i = rmv(engine, bn_i, Bn); //Since it has to be used in some calculations, save it so that it won't be necessary to find it later
			#pragma omp critical 
			{
				Beta.col(i) = beta_i;
				U += (beta_i - mu)*(beta_i - mu).transpose();
			}
			b_tau_eps_post += beta_i.dot(tbase_base*beta_i) - 2*beta_i.dot(tbase_data.col(i));  
		}

		//Precision tau
		b_tau_eps_post /= 2.0;
		tau_eps = rGamma(engine, a_tau_eps_post, 1/b_tau_eps_post);
		

		//Graphical Step
		GGM_method.data_factorized = false; //Need to tell it that matrix U is changing at every iteration and that has to be factorized everytime
		std::tie(K, accepted_mv) = GGM_method(U, n, G, p_addrm, engine); //G is modified inside the function.
		total_accepted += accepted_mv;

		//Check for user interruption
		try{
		  Rcpp::checkUserInterrupt();
		}
		catch(Rcpp::internal::InterruptedException e){ //Close all files before leaving
			Rcpp::Rcout<<"Execution stopped during iter "<<iter<<"/"<<niter<<std::endl;
			H5Dclose(dataset_Graph);
			H5Dclose(dataset_Beta);
			H5Dclose(dataset_TauEps);
			H5Dclose(dataset_Prec);
			H5Dclose(dataset_Mu);
			H5Dclose(dataset_info);
			H5Dclose(dataset_version);
			H5Fclose(file);
			return -1;
		}
		
		//Save
		if(iter >= nburn){
			if((iter - nburn)%thin == 0 && it_saved < iter_to_store){
				//Save on file
				HDF5conversion::AddMatrix(dataset_Beta,   Beta,    it_saved);	
				HDF5conversion::AddVector(dataset_Mu,     mu,      it_saved);	
				HDF5conversion::AddScalar(dataset_TauEps, tau_eps, it_saved);
				it_saved++;

			}
			if((iter - nburn)%thinG == 0 && it_savedG < iter_to_storeG){

				//Save on file
				std::vector<unsigned int> adj_file;
				if constexpr( ! std::is_same_v<T, unsigned int>){
					std::vector<T> adj_nouint(G.get_adj_list());
					adj_file.resize(adj_nouint.size());
					std::transform(adj_nouint.begin(), adj_nouint.end(), adj_file.begin(), [](T x) { return (unsigned int)x;});
				}
				else{
					adj_file = G.get_adj_list();
				}
				VecCol UpperK{utils::get_upper_part(K)};
				HDF5conversion::AddVector(dataset_Prec, UpperK, it_savedG);	
				HDF5conversion::AddUintVector(dataset_Graph, adj_file, it_savedG);
				it_savedG++;

			}
		}
	}

	H5Dclose(dataset_Graph);
	H5Dclose(dataset_Beta);
	H5Dclose(dataset_TauEps);
	H5Dclose(dataset_Prec);
	H5Dclose(dataset_Mu);
	H5Dclose(dataset_info);
	H5Dclose(dataset_version);
	H5Fclose(file);

	return total_accepted;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------





#endif
#ifndef __BLOCKGRAPHCRTP_H__
#define __BLOCKGRAPHCRTP_H__

//#include <Rcpp.h>

#include "include_headers.h"
#include "Groups.h"

template<class D, class T=unsigned int>
class BlockGraphBaseCRTP{
  public:
    using value_type    = T;
    using Adj  			    = std::vector<T>;
    using IdxType		    = std::size_t;
    using GroupsPtr     = std::shared_ptr<const Groups>;
    using Neighbourhood = std::map<unsigned int, std::vector<unsigned int> >;
    using Singleton     = GroupsTraits::InnerContainer;
    using SingletonEig  = Eigen::Matrix<unsigned int, Eigen::Dynamic, 1>; //It is column

    BlockGraphBaseCRTP(GroupsPtr const & _Gr):ptr_groups(_Gr){
      n_singleton = _Gr->get_n_singleton();
    };
    //Getters
    inline Groups get_groups() const{
      return *ptr_groups;
    }
    inline GroupsPtr get_ptr_groups() const{
      return ptr_groups;
    }
    inline unsigned int get_size() const{ //size = number of vertices in block form
      if(ptr_groups != nullptr)
        return ptr_groups->get_n_groups();
      else
        return 0;
    }
    inline unsigned int get_number_singleton() const{
      return n_singleton;
    }
    inline Singleton get_pos_singleton() const{
      return ptr_groups->get_pos_singleton();
    }
    inline SingletonEig get_row_with_singleton() const{
      Singleton sing(ptr_groups->get_pos_singleton());
      SingletonEig ret_obj(SingletonEig::Zero(this->get_size()));
      if(sing.size() == 0)
        return ret_obj;
      else
        for(auto it = sing.cbegin(); it != sing.cend(); ++it)
          ret_obj(*it) = 1;
      return ret_obj;
    }
    inline unsigned int get_complete_size()const{ //number of vertices in complete form
      return ptr_groups->get_n_elements();
    }
    inline unsigned int get_group_size(IdxType const & i)const{
      return ptr_groups->get_group_size(i);
    }
    unsigned int get_n_links() const              {static_cast<D*>(this)->get_n_links();}
    unsigned int get_n_block_links() const        {static_cast<D*>(this)->get_n_block_links();}
    unsigned int get_possible_links() const       {static_cast<D*>(this)->get_possible_links();}
    unsigned int get_possible_block_links() const {static_cast<D*>(this)->get_possible_block_links();}
    //Find
          //inline unsigned int find_group_idx(IdxType const & i)const{
            //return ptr_groups->find(i);
          //}
    inline unsigned int find_group_idx(IdxType const & i)const{
      //return ptr_groups->map_of_indeces[(unsigned int)i];
      auto it = ptr_groups->map_of_indeces.find((unsigned int)i);
      return it->second;
    }
    inline std::vector<unsigned int> find_and_get_group(IdxType const & i) const{
      return ptr_groups->find_and_get(i);
    }
    //Set the entire graph
    void set_graph(Adj const & A)                                 {static_cast<D*>(this)->set_graph(A);}
    void set_graph(Adj&& A)                                       {static_cast<D*>(this)->set_graph(A);}
    void set_empty_graph()                                        {static_cast<D*>(this)->set_empty_graph();}
    void fillRandom(double sparsity = 0.5, unsigned int seed = 0) {static_cast<D*>(this)->fillRandom(sparsity, seed);}
    //Set-Remove single link
    void add_link(IdxType const & i, IdxType const & j)           {static_cast<D*>(this)->add_link(i,j);}
    void remove_link(IdxType const & i, IdxType const & j)        {static_cast<D*>(this)->remove_link(i,j);}
    //Converters
    std::pair<unsigned int, unsigned int> pos_to_ij(IdxType const & pos) const;
    std::vector< std::pair<unsigned int, unsigned int> > map_to_complete(IdxType i, IdxType j) const{ 
      if(i >= this->get_size() || j>=this->get_size() )
        throw std::runtime_error("Invalid index request");
      if(j < i)
        std::swap(i,j);
      std::vector<unsigned int> group_i(ptr_groups->get_group(i));
      std::vector<unsigned int> group_j(ptr_groups->get_group(j));
      return utils::cartesian_product(group_i, group_j);
    }
    // Operators
    T  operator()(IdxType const & i, IdxType const & j)const      {static_cast<D*>(this)->operator()(i,j);}
    //virtual T& operator()(IdxType const & i, IdxType const & j) = 0;
    //Clone
    //virtual std::unique_ptr< BlockGraphBaseCRTP<T> > clone() const = 0;
    //Desctructor
    //virtual ~BlockGraphBaseCRTP() = default;
  protected:
    GroupsPtr ptr_groups;
    unsigned int n_singleton;
    unsigned int n_links;
    unsigned int n_blocks;
    void find_neighbours()        {static_cast<D*>(this)->find_neighbours();}
    void compute_nlinks_nblocks() {static_cast<D*>(this)->compute_nlinks_nblocks();}
    IdxType compute_diagonal_position(IdxType const & i) const;
};



template <class D, class T>
typename BlockGraphBaseCRTP<D,T>::IdxType BlockGraphBaseCRTP<D,T>::compute_diagonal_position(IdxType const & i) const{
  if(i == 0)
    return 0;

  IdxType res = 0;
  #pragma omp parallel for reduction (+ : res)
  for(IdxType k = 0; k < i ; ++k){
    res += (ptr_groups->get_n_groups() - k);
  }
  if(n_singleton != 0){

    std::vector<unsigned int> singleton = ptr_groups->get_pos_singleton();
    res -= std::count_if(singleton.cbegin(), singleton.cend(), [&i](IdxType const & pos_s){return (pos_s <= i);} );
  }
  return res;
}
 
//Questa fa abbastanza schifo ma è difficile da scrivere 
template<class D, class T>
std::pair<unsigned int, unsigned int> 
BlockGraphBaseCRTP<D,T>::pos_to_ij(typename BlockGraphBaseCRTP<D,T>::IdxType const & pos) const{

  using IdxType   = typename BlockGraphBaseCRTP<D,T>::IdxType;
  using Singleton = typename BlockGraphBaseCRTP<D,T>::Singleton;

  if(pos > this->get_possible_block_links())
    throw std::runtime_error("Requested position exceeds matrix dimension");

  Singleton sing(ptr_groups->get_pos_singleton());
  if(pos == 0){
    if(std::find(sing.cbegin(), sing.cend(), 0) == sing.cend()) 
      return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(0,0));
    else
      return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(0,1));
  }
    unsigned int extra;
    (std::find(sing.cbegin(), sing.cend(), 0) == sing.cend()) ? extra=0 : extra=1;
    IdxType cde_old = 0;
    IdxType cde_new = 0;
    IdxType last( this->get_size() - 1 ) ;
    for(IdxType h = 0; h < last; ++h){
      cde_new = compute_diagonal_position(h+1);
      if(pos == cde_old){
        return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(h,h));
      }
      if(pos == cde_new){
        if(std::find(sing.cbegin(), sing.cend(), h+1) == sing.cend()) // (h+1) not a singleton, select diagonal 
          return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(h+1,h+1));
        else
          return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(h,last)); //last element of the row
      }
      if(pos > cde_old && pos < cde_new){
        return (h == 0) ? static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(h, pos - cde_old + h + extra)) : 
                          static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(h, pos - cde_old + h)) ;
      }
      cde_old = cde_new;
    }
  return static_cast<std::pair<unsigned int, unsigned int> >(std::make_pair(last,last));
}

//------------------------------------------------------------------------------------------------------------------------------------------------

template<class T=unsigned int>
class CompleteViewCRTP;

template<class T=unsigned int>
class BlockGraphCRTP : public BlockGraphBaseCRTP<BlockGraphCRTP<T>, T>{
  public:
    using value_type    = typename BlockGraphBaseCRTP<BlockGraphCRTP,T>::value_type;
    using Adj           = typename BlockGraphBaseCRTP<BlockGraphCRTP,T>::Adj;
    using IdxType       = typename BlockGraphBaseCRTP<BlockGraphCRTP,T>::IdxType;
    using InnerData     = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    using GroupsPtr     = typename BlockGraphBaseCRTP<BlockGraphCRTP,T>::GroupsPtr;
    using Neighbourhood = typename BlockGraphBaseCRTP<BlockGraphCRTP,T>::Neighbourhood;
    //Constructors
    BlockGraphCRTP(Adj const & _A, GroupsPtr const & _Gr):BlockGraphBaseCRTP<BlockGraphCRTP,T>(_Gr){
      unsigned int M{this->ptr_groups->get_n_groups()};
      if( M*(M-1)/2 + M - this->n_singleton != _A.size() )
        throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
      else{
        this->fillFromAdj(_A);
        this->find_neighbours();
        this->compute_nlinks_nblocks();
      }
    };
    BlockGraphCRTP(GroupsPtr const & _Gr):BlockGraphBaseCRTP<BlockGraphCRTP,T>(_Gr){};
    BlockGraphCRTP(InnerData const & _Mat, GroupsPtr const & _Gr):data(_Mat),BlockGraphBaseCRTP<BlockGraphCRTP,T>(_Gr){
      if(data.rows() != data.cols())
        throw std::runtime_error("Matrix insereted as graph is not squared");
      if( this->ptr_groups->get_n_groups() != data.rows() )
        throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
      std::vector<unsigned int> sing = this->ptr_groups->get_pos_singleton();
      std::for_each(sing.cbegin(), sing.cend(),[this](unsigned int const & pos){this->data(pos,pos) = true;}); //To ensure sigleton to be set as ones
      this->find_neighbours();
      this->compute_nlinks_nblocks();
    }
    BlockGraphCRTP(Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor> const & _Mat, GroupsPtr const & _Gr):data(_Mat),BlockGraphBaseCRTP<BlockGraphCRTP,T>(_Gr){
      if(data.rows() != data.cols())
        throw std::runtime_error("Matrix insereted as graph is not squared");
      if( this->ptr_groups->get_n_groups() != data.rows() )
        throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
      std::vector<unsigned int> sing = this->ptr_groups->get_pos_singleton();
      std::for_each(sing.cbegin(), sing.cend(),[this](unsigned int const & pos){this->data(pos,pos) = true;}); //To ensure sigleton to be set as ones
      this->find_neighbours();
      this->compute_nlinks_nblocks();
    }
    //BlockGraphCRTP(BlockGraphCRTP const & _Gr); default is ok
    //BlockGraphCRTP(BlockGraphCRTP&& _Gr); default is ok
    //BlockGraphCRTP()=default; NON voglio il default constructor

    //Getters
    inline InnerData get_graph() const{
      return data;
    }
    Adj get_adj_list()const;

    inline Neighbourhood get_neighbours()const{
      return this->neighbours;
    }
    inline unsigned int get_n_links() const{
      return this->n_links;
    }
    inline unsigned int get_n_block_links() const{
      return this->n_blocks;
    }
    inline unsigned int get_possible_links() const{
      return 0.5*this->get_complete_size()*(this->get_complete_size()-1);
    }
    inline unsigned int get_possible_block_links() const{
      return (0.5*this->get_size()*(this->get_size()-1) + this->get_size() - this->get_number_singleton());
    }
    //Set the entire graph
    void set_graph(Adj const & A);//Usage, GraphObj.set_graph(A);
    void set_graph(Adj&& A);    //Usage, GraphObj.set_graph(std::move(A));
    void set_empty_graph();
    void fillRandom(double sparsity = 0.5, unsigned int seed = 0);
    //Set-Remove single link
    inline void add_link(IdxType const & i, IdxType const & j){
      data(i,j) = true;
    }
    inline void remove_link(IdxType const & i, IdxType const & j){
      data(i,j) = false;
    }    
    inline CompleteViewCRTP<T> completeview(){
      return CompleteViewCRTP (*this);
    }
    // Operators
    T operator()(IdxType const & i, IdxType const & j)const{
      return (i<j) ? (data(i,j)) : (data(j,i));
    }
    T& operator()(IdxType const & i, IdxType const & j){
      return (i<j) ? (data(i,j)) : (data(j,i));
    }

    friend std::ostream & operator<<(std::ostream & str, BlockGraphCRTP & G){
      //str<<G.data;
      for(IdxType i = 0; i < G.get_size(); ++i){
        for(IdxType j = 0; j < G.get_size(); ++j){
          if(i <= j)
            std::cout<<G(i,j)<<" ";
          else
            std::cout<<G(j,i)<<" ";
        }
        std::cout<<std::endl;
      }
      return str;
    }
    //Clone
    //virtual std::unique_ptr< BlockGraphBaseCRTP<T> > clone() const{
      //return std::make_unique< BlockGraphCRTP<T> > (*this);
    //};
    void find_neighbours();
  private:
    InnerData data;
    Neighbourhood neighbours;
    void fillFromAdj(Adj const & A);
    void fillFromAdj(Adj&& A);
    void compute_nlinks_nblocks();
};


template<class T>
typename BlockGraphCRTP<T>::Adj BlockGraphCRTP<T>::get_adj_list()const{
  Adj adj_list;
  adj_list.reserve(this->get_possible_block_links());
  unsigned int M{this->ptr_groups->get_n_groups()};
  for(unsigned int i = 0; i < M; ++i){
    for(unsigned int j = i; j < M; ++j){

      if(i == j){
        std::vector<unsigned int> singleton = this->get_pos_singleton();
        auto it = std::find(singleton.cbegin(), singleton.cend(), i);
        if(it == singleton.cend() ){
          //std::cout<<"Not a singleton, I can add"<<std::endl;
          adj_list.emplace_back(data(i,i));
        }
        else{
          //std::cout<<"It's a singleton"<<std::endl;
        }
      }
      else
      {
        adj_list.emplace_back(data(i,j));
      }
    }
  }
  if(adj_list.size() != this->get_possible_block_links())
    std::cout<<"Something wrong i can feel it"<<std::endl;
  return adj_list;
}
template<class T>
void BlockGraphCRTP<T>::set_graph(typename BlockGraphCRTP<T>::Adj const & A){
  unsigned int M{this->ptr_groups->get_n_groups()};
  if( M*(M-1)/2 + M - this->n_singleton != A.size() )
    throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");

  //std::cout<<"Copy?"<<std::endl;
  fillFromAdj(A);
  neighbours.clear();
  this->find_neighbours();
  this->compute_nlinks_nblocks();
}
template<class T>
void BlockGraphCRTP<T>::set_graph(typename BlockGraphCRTP<T>::Adj&& A){

  unsigned int M{this->ptr_groups->get_n_groups()};
  if( M*(M-1)/2 + M - this->n_singleton != A.size() )
    throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
  //std::cout<<"Move?"<<std::endl;
  fillFromAdj(std::move(A));

  // Without the last line, A.size() becomes 0. So if I try to access A[0] I get a Segmentation Fault.
  // If it is needed to avoid this problem, set A.size() = 1. I waste one element but no Segmentation Fault.
  //A.resize(1);
  neighbours.clear();
  this->find_neighbours();
  this->compute_nlinks_nblocks();
}

template<class T>
void BlockGraphCRTP<T>::set_empty_graph(){
  data = InnerData::Zero(this->get_size(), this->get_size());
  std::vector<unsigned int> pos_sing = this->ptr_groups->get_pos_singleton();
  if(pos_sing.size() > 0){
      for(unsigned int i = 0; i < pos_sing.size(); ++i)
          data(pos_sing[i], pos_sing[i]) = true;
  }
  neighbours.clear();
  this->find_neighbours();
  this->compute_nlinks_nblocks();
}
template<class T>
void BlockGraphCRTP<T>::fillRandom(double sparsity, unsigned int seed){

  if(sparsity > 1.0){
    std::cerr<<"Sparsity larger then 1, set to 0.5";
    sparsity = 0.5;
  }
  // If I do not give the sees I initialize the sequence using the random device. Every call will produce a different matrix
  if(seed==0){
    std::random_device rd;
    seed=rd();
  }
  // I use the standard engine for random numbers
  std::default_random_engine engine(seed);
  // A uniform distribution between 0 and 1
  std::uniform_real_distribution<double> rand(0.,1.);

  //fill the graph
  unsigned int M{this->ptr_groups->get_n_groups()};
  data.resize(M,M);
  data = InnerData::Zero(M,M);
  for(unsigned int i = 0; i < M; ++i){
    for(unsigned int j = i; j < M; ++j){

      if(i == j){
        std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
        auto it = std::find(singleton.cbegin(), singleton.cend(), i);
        if(it == singleton.cend() ){
          //std::cout<<"Not a singleton, I can add"<<std::endl;
          (rand(engine) < sparsity) ? data(i,j)=true : data(i,j)=false;
        }
        else{
          //std::cout<<"It's a singleton"<<std::endl;
          data(i,j) = true;
        }
      }
      else
      {
        (rand(engine) < sparsity) ? data(i,j)=true : data(i,j)=false;
      }
    }
  }
  neighbours.clear();
  find_neighbours(); 
  this->compute_nlinks_nblocks();
}
template<class T>
void BlockGraphCRTP<T>::fillFromAdj(Adj const & A){
  //std::cout<<"Copy!"<<std::endl;
  unsigned int M{this->ptr_groups->get_n_groups()};
  data.resize(M,M);
  data = InnerData::Zero(M,M);
  for(unsigned int i = 0; i < M; ++i){
    for(unsigned int j = i; j < M; ++j){
      if(i == j){
        std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
        auto it = std::find(singleton.cbegin(), singleton.cend(), i);
        if(it == singleton.cend() ){
          //std::cout<<"Not a singleton, I can add"<<std::endl;
          data(i,j) = A[this->compute_diagonal_position(i)];
        }
        else{
          //std::cout<<"It's a singleton"<<std::endl;
          data(i,j) = 1;
        }
      }
      else
        data(i,j) = A[this->compute_diagonal_position(i) + (j - i)];
    }
  }
}

template<class T>
void BlockGraphCRTP<T>::fillFromAdj(typename BlockGraphCRTP<T>::Adj&& A){
  //std::cout<<"Move!"<<std::endl;
  unsigned int M{this->ptr_groups->get_n_groups()};
  data.resize(M,M);
  data = InnerData::Zero(M,M);
  for(unsigned int i = 0; i < M; ++i){
    for(unsigned int j = i; j < M; ++j){
      if(i == j){
        std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
        auto it = std::find(singleton.cbegin(), singleton.cend(), i);
        if(it == singleton.cend() ){
          //std::cout<<"Not a singleton, I can add"<<std::endl;
          data(i,j) = A[this->compute_diagonal_position(i)];
        }
        else{
          //std::cout<<"It's a singleton"<<std::endl;
          data(i,j) = 1;
        }
      }
      else
        data(i,j) = A[this->compute_diagonal_position(i) + (j - i)];
    }
  }
  A.clear();
}

template<class T>
void BlockGraphCRTP<T>::find_neighbours(){
  using IdxType = typename BlockGraphCRTP<T>::IdxType;
  
  for(IdxType i=0; i < this->get_complete_size(); ++i){
    std::set<unsigned int> temp; //it is mandatory for my_nbd to be sorted. Insert element in a set and the plug the set into the vector
    unsigned int idx_i = this->find_group_idx(i);
    for(IdxType j = 0; j < this->get_size(); ++j){
      if((*this)(idx_i,j) == true){
        if(idx_i == j){
          std::vector<unsigned int> v(this->ptr_groups->get_group(j));
          std::copy_if(v.begin(), v.end(), std::inserter(temp, temp.begin()), [i](IdxType const & idx){return !(idx==i);} ); 
          //std::copy_if(v.begin(), v.end(), std::inserter(neighbours[i], neighbours[i].end()), [i](IdxType const & idx){return !(idx==i);} ); 
        }
        else{
          std::vector<unsigned int> v(this->ptr_groups->get_group(j)) ;
          std::copy(v.begin(), v.end(), std::inserter(temp, temp.begin())); 
          //std::copy(v.begin(), v.end(), std::inserter(neighbours[i], neighbours[i].end())); 
        }
      }
    }
    std::vector<unsigned int> my_nbds(temp.begin(), temp.end());
    neighbours.insert(std::make_pair(i, my_nbds));
  }
}

template<class T>
void BlockGraphCRTP<T>::compute_nlinks_nblocks(){
  this->n_links  = 0;
  this->n_blocks = 0;
      for(IdxType i = 0; i < this->get_size(); ++i){
        for(IdxType j = i; j < this->get_size(); ++j){
          if( (*this)(i,j) == true){
            this->n_blocks++;
            if(i == j)
              this->n_links += 0.5*this->get_group_size(i)*(this->get_group_size(i) - 1);
            else
              this->n_links += this->get_group_size(i)*this->get_group_size(j);  
          }
          
        } 
      }
      this->n_blocks -= this->get_number_singleton();
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------

template<class T>
class CompleteViewCRTP{
  public:
    using value_type    = typename BlockGraphBaseCRTP<BlockGraphCRTP<T>,T>::value_type;
    using IdxType       = typename BlockGraphBaseCRTP<BlockGraphCRTP<T>,T>::IdxType;
    using Neighbourhood = typename BlockGraphBaseCRTP<BlockGraphCRTP<T>,T>::Neighbourhood;
    //Constructor
    CompleteViewCRTP(BlockGraphCRTP<T> & _G):G(_G){};

    T operator()( typename BlockGraphBaseCRTP<BlockGraphCRTP<T>,T>::IdxType const & i, 
                  typename BlockGraphBaseCRTP<BlockGraphCRTP<T>,T>::IdxType const & j) const
    {
      if(i == j)
        return true;
      else
        return G( G.find_group_idx(i), G.find_group_idx(j) ); 
    }
    //Getters
    inline unsigned int get_size() const{
      return G.get_complete_size();
    }
    inline Neighbourhood get_nbd()const{
      return G.get_neighbours();
    }
    inline std::vector<unsigned int> get_nbd(IdxType const & i)const{
      if(i >= G.get_neighbours().size())
        throw std::runtime_error("Invalid index request");
      else
        return G.get_neighbours()[i];  
    }
    inline unsigned int get_n_links()const{
      return G.get_n_links();
    }
    inline unsigned int get_n_groups() const{ 
        return G.get_size();
    }
    inline std::vector< unsigned int > get_group(IdxType const & i)const{
      return G.get_ptr_groups()->get_group(i);
    }
    inline unsigned int get_n_singleton() const{
      return G.get_number_singleton();
    }
    inline unsigned int get_possible_links() const{
      return G.get_possible_links();
    }
    inline unsigned int get_possible_block_links() const{
      return G.get_possible_block_links();
    }
    inline unsigned int get_n_block_links() const{
      return G.get_n_block_links();
    }
    inline std::vector< std::pair<unsigned int, unsigned int> > map_to_complete(IdxType i, IdxType j) const{
      return G.map_to_complete(i,j);
    }
    inline unsigned int get_group_size(IdxType const & i)const{
      return G.get_group_size(i);
    }

    friend std::ostream & operator<<(std::ostream & str, CompleteViewCRTP & _G){
      for(IdxType i = 0; i < _G.G.get_complete_size(); ++i){
        for(IdxType j = 0; j < _G.G.get_complete_size(); ++j)
          std::cout<<_G(i,j)<<" ";
        std::cout<<std::endl;
      }

      return str;
    }

  private:
    BlockGraphCRTP<T>&  G;
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------

template<class T>
class CompleteViewAdjCRTP;

template<class T = unsigned int>
class BlockGraphAdjCRTP : public BlockGraphBaseCRTP<BlockGraphAdjCRTP<T>, T>{
  public:
    using value_type    = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::value_type;
    using Adj           = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Adj;
    using IdxType       = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType;
    using InnerData     = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Adj;
    using Iterator      = typename InnerData::iterator;
    using ConstIterator = typename InnerData::const_iterator;
    using GroupsPtr     = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::GroupsPtr;
    using Neighbourhood = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Neighbourhood;

    BlockGraphAdjCRTP(GroupsPtr const & _Gr): BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>(_Gr){};
    BlockGraphAdjCRTP(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Adj const & _A, GroupsPtr const & _Gr):BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>(_Gr), data(_A){
      unsigned int M{this->ptr_groups->get_n_groups()};
      if( M*(M-1)/2 + M - this->n_singleton != data.size() ){
        throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
      }
      this->find_neighbours();
      this->compute_nlinks_nblocks();
     }
    //Getters
    inline InnerData get_graph() const{
      return data;
    }
    inline Adj get_adj_list()const{
      return data;
    } 
    inline Neighbourhood get_neighbours()const{
      return this->neighbours;
    }
    unsigned int get_n_links() const{
      return this->n_links;
    }
    unsigned int get_n_block_links() const{
      return this->n_blocks;
    }
    unsigned int get_possible_links() const{
      return 0.5*this->get_complete_size()*(this->get_complete_size()-1);
    }
    unsigned int get_possible_block_links() const{
      return (0.5*this->get_size()*(this->get_size()-1) + this->get_size() - this->get_number_singleton());
    }

    //Set the entire graph
    void set_graph(Adj const & A);//Usage, GraphObj.set_graph(A);
    void set_graph(Adj&& A);    //Usage, GraphObj.set_graph(std::move(A));
    void set_empty_graph(){
      Adj adj_empty(0.5*this->get_size()*(this->get_size()-1) + this->get_size() - this->get_number_singleton(), false);
      this->set_graph(adj_empty);
    }
    void fillRandom(double sparsity = 0.5, unsigned int seed = 0);
    //Set-Remove single link
    inline void add_link(IdxType const & pos){
      if(pos >= data.size())
        throw std::runtime_error("Invalid index request");
      else
        data[pos] = true;
    }
    inline void remove_link(IdxType const & pos){
      if(pos >= data.size())
        throw std::runtime_error("Invalid index request");
      else
        data[pos] = false;
    }
    void add_link(IdxType const & i, IdxType const & j);
    void remove_link(IdxType const & i, IdxType const & j);
    inline CompleteViewAdjCRTP<T> completeview(){
      return CompleteViewAdjCRTP (*this);
    }
    // Operators
    T  operator()(IdxType const & i, IdxType const & j)const;
    //T& operator()(IdxType const & i, IdxType const & j; //Meglio usare add e remove (i,j), questa nel caso in cui selezioni un singleton è un problema

    friend std::ostream & operator<<(std::ostream & str, BlockGraphAdjCRTP const & G){
      if(G.get_size() == 0)
        return str;
      auto it = G.data.cbegin();
      str<<*it;
      it++;
      for(/*it*/; it!=G.data.cend(); ++it)
        str<<", "<<*it;
      return str;
    }
    //Clone
    //virtual std::unique_ptr< ... > clone() const{
      //return std::make_unique< BlockGraphAdjCRTP<T> > (*this);
    //};
  private:
    InnerData  data;
    void find_neighbours();
    void compute_nlinks_nblocks();
    Neighbourhood neighbours;  
};

template<class T>
void BlockGraphAdjCRTP<T>::set_graph(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Adj&& A){

  unsigned int M{this->ptr_groups->get_n_groups()};
  if( M*(M-1)/2 + M - this->n_singleton != A.size() )
    throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");

  //std::cout<<"Move"<<std::endl;
  data = std::move(A);

  // Without the last line, A.size() becomes 0. So if I try to access A[0] I get a Segmentation Fault.
  // If it is needed to avoid this problem, set A.size() = 1. I waste one element but no Segmentation Fault.
  //A.resize(1);
  neighbours.clear();
  this->find_neighbours();
  this->compute_nlinks_nblocks();
}

template<class T>
void BlockGraphAdjCRTP<T>::set_graph(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::Adj const & A){
  unsigned int M{this->ptr_groups->get_n_groups()};
  if( M*(M-1)/2 + M - this->n_singleton != A.size() )
    throw std::runtime_error("The number of groups is not coherent with the size of the adjacency matrix");
  //std::cout<<"Copy"<<std::endl;
  data = A;
  neighbours.clear();
  this->find_neighbours();
  this->compute_nlinks_nblocks();
}

template<class T>
void BlockGraphAdjCRTP<T>::fillRandom(double sparsity, unsigned int seed){

  if(sparsity > 1.0){
    std::cerr<<"Sparsity larger then 1, set to 0.5";
    sparsity = 0.5;
  }
  // If I do not give the sees I initialize the sequence using the random device. Every call will produce a different matrix
  if(seed==0){
    std::random_device rd;
    seed=rd();
  }
  // I use the standard engine for random numbers
  std::default_random_engine engine(seed);
  // A uniform distribution between 0 and 1
  std::uniform_real_distribution<double> rand(0.,1.);

  //fill the graph
  unsigned int M{this->ptr_groups->get_n_groups()};
  data.resize(M*(M-1)/2 + M - this->n_singleton );
  for(Iterator it = data.begin(); it != data.end(); ++it)
    (rand(engine) < sparsity) ? *it=true : *it=false;

  neighbours.clear();
  find_neighbours(); 
  this->compute_nlinks_nblocks();
}

template<class T>
void BlockGraphAdjCRTP<T>::add_link(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & i, typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & j){

  if(i == j){
    std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
    auto it = std::find(singleton.cbegin(), singleton.cend(), i);
    if(it == singleton.cend() ){
      //std::cout<<"Not a singleton, I can add"<<std::endl;
      data[this->compute_diagonal_position(i)] = true;
    }
    else
      std::cerr<<"Cannot add a singleton";
  }
  else{
    (i<j) ? (data[this->compute_diagonal_position(i) + (j-i)] = true) : (data[this->compute_diagonal_position(j) + (i-j)] = true);
  }
}

template<class T>
void BlockGraphAdjCRTP<T>::remove_link(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & i, typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & j){

  if(i == j){
    std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton(); //Perché mi obbliga a definirlo fuorii??
    auto it = std::find(singleton.cbegin(), singleton.cend(), i);
    if(it == singleton.cend() ){
      //std::cout<<"Not a singleton, I can add"<<std::endl;
      data[this->compute_diagonal_position(i)] = false;
    }
    else
      std::cerr<<"Cannot remove a singleton";
  }
  else{
    (i<j) ? (data[this->compute_diagonal_position(i) + (j-i)] = false) : (data[this->compute_diagonal_position(j) + (i-j)] = false);
  }
}

template<class T>
T BlockGraphAdjCRTP<T>::operator()(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & i, 
                                   typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & j)const
{
  if(i > this->get_size() || j > this->get_size())
    throw std::runtime_error("Invalid index request");
  if(i == j){
    std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
    auto it = std::find(singleton.cbegin(), singleton.cend(), i);
    if(it == singleton.cend() )
      return data[this->compute_diagonal_position(i)];
    else{
      //std::cerr<<"Singleton";
      return true;
    }
  }
  else
    return (i < j) ? data[this->compute_diagonal_position(i) + (j-i)] : data[this->compute_diagonal_position(j) + (i-j)];
}

//template<class T>
//T& BlockGraphAdjCRTP<T>::operator()(typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & i, typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType const & j){
//
  //if(i == j){
    //std::vector<unsigned int> singleton = this->ptr_groups->get_pos_singleton();
    //auto it = std::find(singleton.cbegin(), singleton.cend(), i);
    //if(it == singleton.cend() ){
      ////std::cout<<"Not a singleton, I can add"<<std::endl;
      //return data[this->compute_diagonal_position(i)];
    //}
    //else{
      //std::cout<<"Cannot add a singleton"<<std::endl;
      //throw std::runtime_error("Cannot address a singleton");
    //}
  //}
  //else{
    //return  (i<j) ? (data[this->compute_diagonal_position(i) + (j-i)]) : (data[this->compute_diagonal_position(j) + (i-j)]);
  //}
//}

template<class T>
void BlockGraphAdjCRTP<T>::find_neighbours(){
  using IdxType = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP,T>::IdxType;

  for(IdxType i=0; i < this->get_complete_size(); ++i){
    //It is mandatory for my_nbd to be sorted and without repetitions. Element are indeed inserted in a set and the plug the set into the vector
    std::set<unsigned int> temp; 
    unsigned int idx_i = this->find_group_idx(i);
    for(IdxType j = 0; j < this->get_size(); ++j){
      if((*this)(idx_i,j) == true){
        if(idx_i == j){
          std::vector<unsigned int> v(this->ptr_groups->get_group(j));
          std::copy_if(v.begin(), v.end(), std::inserter(temp, temp.begin()), [i](IdxType const & idx){return !(idx==i);} ); 
        }
        else{
          std::vector<unsigned int> v(this->ptr_groups->get_group(j)) ;
          std::copy(v.begin(), v.end(), std::inserter(temp, temp.begin())); 
        }
      }
    }
    std::vector<unsigned int> my_nbds(temp.begin(), temp.end());
    neighbours.insert(std::make_pair(i, my_nbds));
  }
}

template<class T>
void BlockGraphAdjCRTP<T>::compute_nlinks_nblocks(){
  this->n_links  = 0;
  this->n_blocks = 0;
      for(IdxType i = 0; i < this->get_size(); ++i){
        for(IdxType j = i; j < this->get_size(); ++j){
          if( (*this)(i,j) == true){
            this->n_blocks++;
            if(i == j)
              this->n_links += 0.5*this->get_group_size(i)*(this->get_group_size(i) - 1);
            else
              this->n_links += this->get_group_size(i)*this->get_group_size(j);  
          }
          
        } 
      }
      this->n_blocks -= this->get_number_singleton();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

template<class T>
class CompleteViewAdjCRTP{
  public:
    using value_type    = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP<T>,T>::value_type;
    using IdxType       = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP<T>,T>::IdxType;
    using Neighbourhood = typename BlockGraphBaseCRTP<BlockGraphAdjCRTP<T>,T>::Neighbourhood;
    //Constructor
    CompleteViewAdjCRTP(BlockGraphAdjCRTP<T> & _G):G(_G){};

    T operator()(typename BlockGraphBase<T>::IdxType const & i, typename BlockGraphBase<T>::IdxType const & j) const{
      if(i == j)
        return true;
      else
        return G( G.find_group_idx(i), G.find_group_idx(j) );
    }
    friend std::ostream & operator<<(std::ostream & str, CompleteViewAdjCRTP & _G){
      for(IdxType i = 0; i < _G.G.get_complete_size(); ++i)
        for(IdxType j = i; j < _G.G.get_complete_size(); ++j)
          std::cout<<_G(i,j);
      std::cout<<std::endl;
      return str;
    }
    //Getters
    unsigned int get_size() const{
      return G.get_complete_size();
    }
    inline Neighbourhood get_nbd()const{
      return G.get_neighbours();
    }
    inline std::vector<unsigned int> get_nbd(IdxType const & i)const{
      if(i >= G.get_neighbours().size())
        throw std::runtime_error("Invalid index request");
      else
        return G.get_neighbours()[i];   
    }
    inline unsigned int get_n_links()const{
      return G.get_n_links();
    }
    inline unsigned int get_n_groups() const{ 
        return G.get_size();
    }
    inline std::vector< unsigned int > get_group(IdxType const & i)const{
      return G.get_ptr_groups()->get_group(i);
    }
    inline unsigned int get_n_singleton() const{
      return G.get_number_singleton();
    }
    inline unsigned int get_possible_links() const{
      return G.get_possible_links();
    }
    inline unsigned int get_possible_block_links() const{
      return G.get_possible_block_links();
    }
    inline unsigned int get_n_block_links() const{
      return G.get_n_block_links();
    }
    inline std::vector< std::pair<unsigned int, unsigned int> > map_to_complete(IdxType i, IdxType j) const{
      return G.map_to_complete(i,j);
    }
    inline unsigned int get_group_size(IdxType const & i)const{
      return G.get_group_size(i);
    }
  private:
    BlockGraphAdjCRTP<T> & G;
};

#endif
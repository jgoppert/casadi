/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010-2014 Joel Andersson, Joris Gillis, Moritz Diehl,
 *                            K.U. Leuven. All rights reserved.
 *    Copyright (C) 2011-2014 Greg Horn
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef CASADI_X_FUNCTION_HPP
#define CASADI_X_FUNCTION_HPP

#include <stack>
#include "function_internal.hpp"

// To reuse variables we need to be able to sort by sparsity pattern
#include <unordered_map>
#define SPARSITY_MAP std::unordered_map

/// \cond INTERNAL

namespace casadi {

  /** \brief  Internal node class for the base class of SXFunction and MXFunction
      (lacks a public counterpart)
      The design of the class uses the curiously recurring template pattern (CRTP) idiom
      \author Joel Andersson
      \date 2011
  */
  template<typename DerivedType, typename MatType, typename NodeType>
  class CASADI_EXPORT XFunction : public FunctionInternal {
  public:

    /** \brief  Constructor  */
    XFunction(const std::string& name,
              const std::vector<MatType>& inputv,
              const std::vector<MatType>& outputv);

    /** \brief  Destructor */
    virtual ~XFunction() {}

    /** \brief  Initialize */
    virtual void init(const Dict& opts);

    /** \brief  Topological sorting of the nodes based on Depth-First Search (DFS) */
    static void sort_depth_first(std::stack<NodeType*>& s, std::vector<NodeType*>& nodes);

    /** \brief  Topological (re)sorting of the nodes based on Breadth-First Search (BFS)
        (Kahn 1962) */
    static void resort_breadth_first(std::vector<NodeType*>& algnodes);

    /** \brief  Topological (re)sorting of the nodes with the purpose of postponing every
        calculation as much as possible, as long as it does not influence a dependent node */
    static void resort_postpone(std::vector<NodeType*>& algnodes, std::vector<int>& lind);

    /** \brief Gradient via source code transformation */
    MatType grad(int iind=0, int oind=0);

    /** \brief Tangent via source code transformation */
    MatType tang(int iind=0, int oind=0);

    /** \brief  Construct a complete Jacobian by compression */
    MatType jac(int iind=0, int oind=0, bool compact=false, bool symmetric=false,
                bool always_inline=true, bool never_inline=false);

    /** \brief Check if the function is of a particular type */
    virtual bool is_a(const std::string& type, bool recursive) const {
      return type=="xfunction" || (recursive && FunctionInternal::is_a(type, recursive));
    }

    // Factory
    virtual Function factory(const std::string& name,
                             const std::vector<std::string>& s_in,
                             const std::vector<std::string>& s_out,
                             const Function::AuxOut& aux,
                             const Dict& opts) const;

    /** \brief Which variables enter nonlinearly */
    virtual std::vector<bool> nl_var(const std::string& s_in,
                                    const std::vector<std::string>& s_out) const;

    /** \brief Return gradient function  */
    virtual Function getGradient(const std::string& name, int iind, int oind, const Dict& opts);

    /** \brief Return tangent function  */
    virtual Function getTangent(const std::string& name, int iind, int oind, const Dict& opts);

    /** \brief Return Jacobian function  */
    virtual Function getJacobian(const std::string& name, int iind, int oind,
                                 bool compact, bool symmetric, const Dict& opts);

    ///@{
    /** \brief Generate a function that calculates \a nfwd forward derivatives */
    virtual Function get_forward_old(const std::string& name, int nfwd, Dict& opts);
    virtual int get_n_forward() const { return 64;}
    ///@}

    ///@{
    /** \brief Generate a function that calculates \a nadj adjoint derivatives */
    virtual Function get_reverse_old(const std::string& name, int nadj, Dict& opts);
    virtual int get_n_reverse() const { return 64;}
    ///@}

    /** \brief Generate code for the declarations of the C function */
    virtual void generateDeclarations(CodeGenerator& g) const = 0;

    /** \brief Generate code for the body of the C function */
    virtual void generateBody(CodeGenerator& g) const = 0;

    /** \brief Helper function: Check if a vector equals inputv */
    virtual bool isInput(const std::vector<MatType>& arg) const;

    /** \brief Create call to (cached) derivative function, forward mode  */
    void forward_x(const std::vector<MatType>& arg, const std::vector<MatType>& res,
                   const std::vector<std::vector<MatType> >& fseed,
                   std::vector<std::vector<MatType> >& fsens);

    /** \brief Create call to (cached) derivative function, reverse mode  */
    void reverse_x(const std::vector<MatType>& arg, const std::vector<MatType>& res,
                   const std::vector<std::vector<MatType> >& aseed,
                   std::vector<std::vector<MatType> >& asens);
    ///@{
    /** \brief Number of function inputs and outputs */
    virtual size_t get_n_in() { return in_.size(); }
    virtual size_t get_n_out() { return out_.size(); }
    ///@}

    /// @{
    /** \brief Sparsities of function inputs and outputs */
    virtual Sparsity get_sparsity_in(int i) { return in_.at(i).sparsity();}
    virtual Sparsity get_sparsity_out(int i) { return out_.at(i).sparsity();}
    /// @}

    // Data members (all public)

    /** \brief  Inputs of the function (needed for symbolic calculations) */
    std::vector<MatType> in_;

    /** \brief  Outputs of the function (needed for symbolic calculations) */
    std::vector<MatType> out_;
  };

  // Template implementations

  template<typename DerivedType, typename MatType, typename NodeType>
  XFunction<DerivedType, MatType, NodeType>::
  XFunction(const std::string& name,
            const std::vector<MatType>& inputv,
            const std::vector<MatType>& outputv)
    : FunctionInternal(name), in_(inputv),  out_(outputv) {
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::init(const Dict& opts) {
    // Call the init function of the base class
    FunctionInternal::init(opts);

    // Make sure that inputs are symbolic
    for (int i=0; i<n_in(); ++i) {
      if (in_.at(i).nnz()>0 && !in_.at(i).is_valid_input()) {
        casadi_error("XFunction::XFunction: Xfunction input arguments must be"
                     " purely symbolic." << std::endl
                     << "Argument " << i << "(" << name_in(i) << ") is not symbolic.");
      }
    }

    // Check for duplicate entries among the input expressions
    bool has_duplicates = false;
    for (auto&& i : in_) {
      if (i.has_duplicates()) {
        has_duplicates = true;
        break;
      }
    }

    // Reset temporaries
    for (auto&& i : in_) i.resetInput();

    if (has_duplicates) {
      userOut<true, PL_WARN>() << "Input expressions:" << std::endl;
      for (int iind=0; iind<in_.size(); ++iind) {
        userOut<true, PL_WARN>() << iind << ": " << in_[iind] << std::endl;
      }
      casadi_error("The input expressions are not independent (or were not reset properly).");
    }
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::sort_depth_first(
      std::stack<NodeType*>& s, std::vector<NodeType*>& nodes) {
    while (!s.empty()) {

      // Get the topmost element
      NodeType* t = s.top();

      // If the last element on the stack has not yet been added
      if (t && !t->temp) {

        // Find out which not yet added dependency has most number of dependencies
        int max_deps = -1, dep_with_max_deps = -1;
        for (int i=0; i<t->ndep(); ++i) {
          if (t->dep(i).get() !=0 && static_cast<NodeType*>(t->dep(i).get())->temp == 0) {
            int ndep_i = t->dep(i)->ndep();
            if (ndep_i>max_deps) {
              max_deps = ndep_i;
              dep_with_max_deps = i;
            }
          }
        }

        // If there is any dependency which has not yet been added
        if (dep_with_max_deps>=0) {

          // Add to the stack the dependency with the most number of dependencies
          // (so that constants, inputs etc are added last)
          s.push(static_cast<NodeType*>(t->dep(dep_with_max_deps).get()));

        } else {

          // if no dependencies need to be added, we can add the node to the algorithm
          nodes.push_back(t);

          // Mark the node as found
          t->temp = 1;

          // Remove from stack
          s.pop();
        }
      } else {
        // If the last element on the stack has already been added
        s.pop();
      }
    }
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::resort_postpone(
      std::vector<NodeType*>& algnodes, std::vector<int>& lind) {
    // Number of levels
    int nlevels = lind.size()-1;

    // Set the counter to be the corresponding place in the algorithm
    for (int i=0; i<algnodes.size(); ++i)
      algnodes[i]->temp = i;

    // Save the level of each element
    std::vector<int> level(algnodes.size());
    for (int i=0; i<nlevels; ++i)
      for (int j=lind[i]; j<lind[i+1]; ++j)
        level[j] = i;

    // Count the number of times each node is referenced inside the algorithm
    std::vector<int> numref(algnodes.size(), 0);
    for (int i=0; i<algnodes.size(); ++i) {
      for (int c=0; c<algnodes[i]->ndep(); ++c) { // for both children
        NodeType* child = static_cast<NodeType*>(algnodes[i]->dep(c).get());
        if (child && child->hasDep())
          numref[child->temp]++;
      }
    }

    // Stacks of additional nodes at the current and previous level
    std::stack<int> extra[2];

    // Loop over the levels in reverse order
    for (int i=nlevels-1; i>=0; --i) {

      // The stack for the current level (we are removing elements from this stack)
      std::stack<int>& extra_this = extra[i%2]; // i odd -> use extra[1]

      // The stack for the previous level (we are adding elements to this stack)
      std::stack<int>& extra_prev = extra[1-i%2]; // i odd -> use extra[0]

      // Loop over the nodes of the level
      for (int j=lind[i]; j<lind[i+1]; ++j) {
        // element to be treated
        int el = j;

        // elements in the stack have priority
        if (!extra_this.empty()) {
          // Replace the element with one from the stack
          el = extra_this.top();
          extra_this.pop();
          --j; // redo the loop
        }

        // Skip the element if belongs to a higher level (i.e. was already treated)
        if (level[el] > i) continue;

        // for both children
        for (int c=0; c<algnodes[el]->ndep(); ++c) {

          NodeType* child = static_cast<NodeType*>(algnodes[el]->dep(c).get());

          if (child && child->hasDep()) {
            // Decrease the reference count of the children
            numref[child->temp]--;

            // If this was the last time the child was referenced ...
            // ... and it is not the previous level...
            if (numref[child->temp]==0 && level[child->temp] != i-1) {

              // ... then assign a new level ...
              level[child->temp] = i-1;

              // ... and add to stack
              extra_prev.push(child->temp);

            } // if no more references
          } // if binary
        } // for c = ...
      } // for j
    } // for i

    // Count the number of elements on each level
    for (std::vector<int>::iterator it=lind.begin(); it!=lind.end(); ++it)
      *it = 0;
    for (std::vector<int>::const_iterator it=level.begin(); it!=level.end(); ++it)
      lind[*it + 1]++;

    // Cumsum to get the index corresponding to the first element of each level
    for (int i=0; i<nlevels; ++i)
      lind[i+1] += lind[i];

    // New index for each element
    std::vector<int> runind = lind; // running index for each level
    std::vector<int> newind(algnodes.size());
    for (int i=0; i<algnodes.size(); ++i)
      newind[i] = runind[level[algnodes[i]->temp]]++;

    // Resort the algorithm and reset the temporary
    std::vector<NodeType*> oldalgnodes = algnodes;
    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[newind[i]] = oldalgnodes[i];
      oldalgnodes[i]->temp = 0;
    }

  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::resort_breadth_first(
      std::vector<NodeType*>& algnodes) {
    // We shall assign a "level" to each element of the algorithm.
    // A node which does not depend on other binary nodes are assigned
    // level 0 and for nodes that depend on other nodes of the algorithm,
    // the level will be the maximum level of any of the children plus 1.
    // Note that all nodes of a level can be evaluated in parallel.
    // The level will be saved in the temporary variable

    // Total number of levels
    int nlevels = 0;

    // Get the earliest possible level
    for (typename std::vector<NodeType*>::iterator it=algnodes.begin(); it!=algnodes.end(); ++it) {
      // maximum level of any of the children
      int maxlevel = -1;
      for (int c=0; c<(*it)->ndep(); ++c) {    // Loop over the children
        NodeType* child = static_cast<NodeType*>((*it)->dep(c).get());
        if (child->hasDep() && child->temp > maxlevel)
          maxlevel = child->temp;
      }

      // Save the level of this element
      (*it)->temp = 1 + maxlevel;

      // Save if new maximum reached
      if (1 + maxlevel > nlevels)
        nlevels = 1 + maxlevel;
    }
    nlevels++;

    // Index of the first node on each level
    std::vector<int> lind;

    // Count the number of elements on each level
    lind.resize(nlevels+1, 0); // all zeros to start with
    for (int i=0; i<algnodes.size(); ++i)
      lind[algnodes[i]->temp+1]++;

    // Cumsum to get the index of the first node on each level
    for (int i=0; i<nlevels; ++i)
      lind[i+1] += lind[i];

    // Get a new index for each element of the algorithm
    std::vector<int> runind = lind; // running index for each level
    std::vector<int> newind(algnodes.size());
    for (int i=0; i<algnodes.size(); ++i)
      newind[i] = runind[algnodes[i]->temp]++;

    // Resort the algorithm accordingly and reset the temporary
    std::vector<NodeType*> oldalgnodes = algnodes;
    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[newind[i]] = oldalgnodes[i];
      oldalgnodes[i]->temp = 0;
    }

#if 0

    int maxl=-1;
    for (int i=0; i<lind.size()-1; ++i) {
      int l = (lind[i+1] - lind[i]);
      //if (l>10)    userOut() << "#level " << i << ": " << l << std::endl;
      userOut() << l << ", ";
      if (l>maxl) maxl= l;
    }
    userOut() << std::endl << "maxl = " << maxl << std::endl;

    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[i]->temp = i;
    }


    maxl=-1;
    for (int i=0; i<lind.size()-1; ++i) {
      int l = (lind[i+1] - lind[i]);
      userOut() << std::endl << "#level " << i << ": " << l << std::endl;

      int ii = 0;

      for (int j=lind[i]; j<lind[i+1]; ++j) {

        std::vector<NodeType*>::const_iterator it = algnodes.begin() + j;

        userOut() << "  "<< ii++ << ": ";

        int op = (*it)->op;
        stringstream s, s0, s1;
        s << "i_" << (*it)->temp;

        int i0 = (*it)->child[0].get()->temp;
        int i1 = (*it)->child[1].get()->temp;

        if ((*it)->child[0]->hasDep())  s0 << "i_" << i0;
        else                             s0 << (*it)->child[0];
        if ((*it)->child[1]->hasDep())  s1 << "i_" << i1;
        else                             s1 << (*it)->child[1];

        userOut() << s.str() << " = ";
        print_c[op](userOut(), s0.str(), s1.str());
        userOut() << ";" << std::endl;




      }

      userOut() << l << ", ";
      if (l>maxl) maxl= l;
    }
    userOut() << std::endl << "maxl (before) = " << maxl << std::endl;


    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[i]->temp = 0;
    }


#endif

    // Resort in order to postpone all calculations as much as possible, thus saving cache
    resort_postpone(algnodes, lind);


#if 0

    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[i]->temp = i;
    }



    maxl=-1;
    for (int i=0; i<lind.size()-1; ++i) {
      int l = (lind[i+1] - lind[i]);
      userOut() << std::endl << "#level " << i << ": " << l << std::endl;

      int ii = 0;

      for (int j=lind[i]; j<lind[i+1]; ++j) {

        std::vector<NodeType*>::const_iterator it = algnodes.begin() + j;

        userOut() << "  "<< ii++ << ": ";

        int op = (*it)->op;
        stringstream s, s0, s1;
        s << "i_" << (*it)->temp;

        int i0 = (*it)->child[0].get()->temp;
        int i1 = (*it)->child[1].get()->temp;

        if ((*it)->child[0]->hasDep())  s0 << "i_" << i0;
        else                             s0 << (*it)->child[0];
        if ((*it)->child[1]->hasDep())  s1 << "i_" << i1;
        else                             s1 << (*it)->child[1];

        userOut() << s.str() << " = ";
        print_c[op](userOut(), s0.str(), s1.str());
        userOut() << ";" << std::endl;




      }

      userOut() << l << ", ";
      if (l>maxl) maxl= l;
    }
    userOut() << std::endl << "maxl = " << maxl << std::endl;


    //  return;




    for (int i=0; i<algnodes.size(); ++i) {
      algnodes[i]->temp = 0;
    }



    /*assert(0);*/
#endif

  }

  template<typename DerivedType, typename MatType, typename NodeType>
  MatType XFunction<DerivedType, MatType, NodeType>::grad(int iind, int oind) {
    casadi_assert_message(sparsity_out(oind).is_scalar(),
                          "Only gradients of scalar functions allowed. Use jacobian instead.");

    // Quick return if trivially empty
    if (nnz_in(iind)==0 || nnz_out(oind)==0 ||
       sparsity_jac(iind, oind, true, false).nnz()==0) {
      return MatType(size_in(iind));
    }

    // Adjoint seeds
    typename std::vector<std::vector<MatType> > aseed(1, std::vector<MatType>(out_.size()));
    for (int i=0; i<out_.size(); ++i) {
      if (i==oind) {
        aseed[0][i] = MatType::ones(out_[i].sparsity());
      } else {
        aseed[0][i] = MatType::zeros(out_[i].sparsity());
      }
    }

    // Adjoint sensitivities
    std::vector<std::vector<MatType> > asens(1, std::vector<MatType>(in_.size()));
    for (int i=0; i<in_.size(); ++i) {
      asens[0][i] = MatType::zeros(in_[i].sparsity());
    }

    // Calculate with adjoint mode AD
    reverse_x(in_, out_, aseed, asens);

    int dir = 0;
    for (int i=0; i<n_in(); ++i) { // Correct sparsities #1025
      if (asens[dir][i].sparsity()!=in_[i].sparsity()) {
        asens[dir][i] = project(asens[dir][i], in_[i].sparsity());
      }
    }

    // Return adjoint directional derivative
    return asens[0].at(iind);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  MatType XFunction<DerivedType, MatType, NodeType>::tang(int iind, int oind) {
    casadi_assert_message(sparsity_in(iind).is_scalar(),
                          "Only tangent of scalar input functions allowed. Use jacobian instead.");

    // Forward seeds
    typename std::vector<std::vector<MatType> > fseed(1, std::vector<MatType>(in_.size()));
    for (int i=0; i<in_.size(); ++i) {
      if (i==iind) {
        fseed[0][i] = MatType::ones(in_[i].sparsity());
      } else {
        fseed[0][i] = MatType::zeros(in_[i].sparsity());
      }
    }

    // Forward sensitivities
    std::vector<std::vector<MatType> > fsens(1, std::vector<MatType>(out_.size()));
    for (int i=0; i<out_.size(); ++i) {
      fsens[0][i] = MatType::zeros(out_[i].sparsity());
    }

    // Calculate with forward mode AD
    forward(in_, out_, fseed, fsens, true, false);

    // Return adjoint directional derivative
    return fsens[0].at(oind);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  MatType XFunction<DerivedType, MatType, NodeType>
  ::jac(int iind, int oind, bool compact, bool symmetric, bool always_inline, bool never_inline) {
    using namespace std;
    if (verbose()) userOut() << "XFunction::jac begin" << std::endl;

    // Quick return if trivially empty
    if (nnz_in(iind)==0 || nnz_out(oind)==0) {
      std::pair<int, int> jac_shape;
      jac_shape.first = compact ? nnz_out(oind) : numel_out(oind);
      jac_shape.second = compact ? nnz_in(iind) : numel_in(iind);
      return MatType(jac_shape);
    }

    if (symmetric) {
      casadi_assert(sparsity_out(oind).is_dense());
    }

    // Create return object
    MatType ret = MatType::zeros(sparsity_jac(iind, oind, compact, symmetric).T());
    if (verbose()) userOut() << "XFunction::jac allocated return value" << std::endl;

    // Quick return if empty
    if (ret.nnz()==0) {
      return ret.T();
    }

    // Get a bidirectional partition
    Sparsity D1, D2;
    getPartition(iind, oind, D1, D2, true, symmetric);
    if (verbose()) userOut() << "XFunction::jac graph coloring completed" << std::endl;

    // Get the number of forward and adjoint sweeps
    int nfdir = D1.is_null() ? 0 : D1.size2();
    int nadir = D2.is_null() ? 0 : D2.size2();

    // Number of derivative directions supported by the function
    int max_nfdir = optimized_num_dir;
    int max_nadir = optimized_num_dir;

    // Current forward and adjoint direction
    int offset_nfdir = 0, offset_nadir = 0;

    // Evaluation result (known)
    std::vector<MatType> res(out_);

    // Forward and adjoint seeds and sensitivities
    std::vector<std::vector<MatType> > fseed, aseed, fsens, asens;

    // Get the sparsity of the Jacobian block
    Sparsity jsp = sparsity_jac(iind, oind, true, symmetric).T();
    const int* jsp_colind = jsp.colind();
    const int* jsp_row = jsp.row();

    // Input sparsity
    std::vector<int> input_col = sparsity_in(iind).get_col();
    const int* input_row = sparsity_in(iind).row();

    // Output sparsity
    std::vector<int> output_col = sparsity_out(oind).get_col();
    const int* output_row = sparsity_out(oind).row();

    // Get transposes and mappings for jacobian sparsity pattern if we are using forward mode
    if (verbose())   userOut() << "XFunction::jac transposes and mapping" << std::endl;
    std::vector<int> mapping;
    Sparsity jsp_trans;
    if (nfdir>0) {
      jsp_trans = jsp.transpose(mapping);
    }

    // The nonzeros of the sensitivity matrix
    std::vector<int> nzmap, nzmap2;

    // Additions to the jacobian matrix
    std::vector<int> adds, adds2;

    // Temporary vector
    std::vector<int> tmp;

    // Progress
    int progress = -10;

    // Number of sweeps
    int nsweep_fwd = nfdir/max_nfdir;   // Number of sweeps needed for the forward mode
    if (nfdir%max_nfdir>0) nsweep_fwd++;
    int nsweep_adj = nadir/max_nadir;   // Number of sweeps needed for the adjoint mode
    if (nadir%max_nadir>0) nsweep_adj++;
    int nsweep = std::max(nsweep_fwd, nsweep_adj);
    if (verbose())   userOut() << "XFunction::jac " << nsweep << " sweeps needed for "
                              << nfdir << " forward and " << nadir << " adjoint directions"
                              << std::endl;

    // Sparsity of the seeds
    vector<int> seed_col, seed_row;

    // Evaluate until everything has been determined
    for (int s=0; s<nsweep; ++s) {
      // Print progress
      if (verbose()) {
        int progress_new = (s*100)/nsweep;
        // Print when entering a new decade
        if (progress_new / 10 > progress / 10) {
          progress = progress_new;
          userOut() << progress << " %"  << std::endl;
        }
      }

      // Number of forward and adjoint directions in the current "batch"
      int nfdir_batch = std::min(nfdir - offset_nfdir, max_nfdir);
      int nadir_batch = std::min(nadir - offset_nadir, max_nadir);

      // Forward seeds
      fseed.resize(nfdir_batch);
      for (int d=0; d<nfdir_batch; ++d) {
        // Nonzeros of the seed matrix
        seed_col.clear();
        seed_row.clear();

        // For all the directions
        for (int el = D1.colind(offset_nfdir+d); el<D1.colind(offset_nfdir+d+1); ++el) {

          // Get the direction
          int c = D1.row(el);

          // Give a seed in the direction
          seed_col.push_back(input_col[c]);
          seed_row.push_back(input_row[c]);
        }

        // initialize to zero
        fseed[d].resize(n_in());
        for (int ind=0; ind<fseed[d].size(); ++ind) {
          int nrow = size1_in(ind), ncol = size2_in(ind); // Input dimensions
          if (ind==iind) {
            fseed[d][ind] = MatType::ones(Sparsity::triplet(nrow, ncol, seed_row, seed_col));
          } else {
            fseed[d][ind] = MatType(nrow, ncol);
          }
        }
      }

      // Adjoint seeds
      aseed.resize(nadir_batch);
      for (int d=0; d<nadir_batch; ++d) {
        // Nonzeros of the seed matrix
        seed_col.clear();
        seed_row.clear();

        // For all the directions
        for (int el = D2.colind(offset_nadir+d); el<D2.colind(offset_nadir+d+1); ++el) {

          // Get the direction
          int c = D2.row(el);

          // Give a seed in the direction
          seed_col.push_back(output_col[c]);
          seed_row.push_back(output_row[c]);
        }

        //initialize to zero
        aseed[d].resize(n_out());
        for (int ind=0; ind<aseed[d].size(); ++ind) {
          int nrow = size1_out(ind), ncol = size2_out(ind); // Output dimensions
          if (ind==oind) {
            aseed[d][ind] = MatType::ones(Sparsity::triplet(nrow, ncol, seed_row, seed_col));
          } else {
            aseed[d][ind] = MatType(nrow, ncol);
          }
        }
      }

      // Forward sensitivities
      fsens.resize(nfdir_batch);
      for (int d=0; d<nfdir_batch; ++d) {
        // initialize to zero
        fsens[d].resize(n_out());
        for (int oind=0; oind<fsens[d].size(); ++oind) {
          fsens[d][oind] = MatType::zeros(sparsity_out(oind));
        }
      }

      // Adjoint sensitivities
      asens.resize(nadir_batch);
      for (int d=0; d<nadir_batch; ++d) {
        // initialize to zero
        asens[d].resize(n_in());
        for (int ind=0; ind<asens[d].size(); ++ind) {
          asens[d][ind] = MatType::zeros(sparsity_in(ind));
        }
      }

      // Evaluate symbolically
      if (verbose()) userOut() << "XFunction::jac making function call" << std::endl;
      if (fseed.size()>0) {
        casadi_assert(aseed.size()==0);
        forward(in_, out_, fseed, fsens, always_inline, never_inline);
      } else if (aseed.size()>0) {
        casadi_assert(fseed.size()==0);
        reverse(in_, out_, aseed, asens, always_inline, never_inline);
      }

      // Carry out the forward sweeps
      for (int d=0; d<nfdir_batch; ++d) {

        // If symmetric, see how many times each output appears
        if (symmetric) {
          // Initialize to zero
          tmp.resize(nnz_out(oind));
          fill(tmp.begin(), tmp.end(), 0);

          // "Multiply" Jacobian sparsity by seed vector
          for (int el = D1.colind(offset_nfdir+d); el<D1.colind(offset_nfdir+d+1); ++el) {

            // Get the input nonzero
            int c = D1.row(el);

            // Propagate dependencies
            for (int el_jsp=jsp_colind[c]; el_jsp<jsp_colind[c+1]; ++el_jsp) {
              tmp[jsp_row[el_jsp]]++;
            }
          }
        }

        // Locate the nonzeros of the forward sensitivity matrix
        sparsity_out(oind).find(nzmap);
        fsens[d][oind].sparsity().get_nz(nzmap);

        if (symmetric) {
          sparsity_in(iind).find(nzmap2);
          fsens[d][oind].sparsity().get_nz(nzmap2);
        }

        // Assignments to the Jacobian
        adds.resize(fsens[d][oind].nnz());
        fill(adds.begin(), adds.end(), -1);
        if (symmetric) {
          adds2.resize(adds.size());
          fill(adds2.begin(), adds2.end(), -1);
        }

        // For all the input nonzeros treated in the sweep
        for (int el = D1.colind(offset_nfdir+d); el<D1.colind(offset_nfdir+d+1); ++el) {

          // Get the input nonzero
          int c = D1.row(el);
          //int f2_out;
          //if (symmetric) {
          //  f2_out = nzmap2[c];
          //}

          // Loop over the output nonzeros corresponding to this input nonzero
          for (int el_out = jsp_trans.colind(c); el_out<jsp_trans.colind(c+1); ++el_out) {

            // Get the output nonzero
            int r_out = jsp_trans.row(el_out);

            // Get the forward sensitivity nonzero
            int f_out = nzmap[r_out];
            if (f_out<0) continue; // Skip if structurally zero

            // The nonzero of the Jacobian now treated
            int elJ = mapping[el_out];

            if (symmetric) {
              if (tmp[r_out]==1) {
                adds[f_out] = el_out;
                adds2[f_out] = elJ;
              }
            } else {
              // Get the output seed
              adds[f_out] = elJ;
            }
          }
        }

        // Get entries in fsens[d][oind] with nonnegative indices
        tmp.resize(adds.size());
        int sz = 0;
        for (int i=0; i<adds.size(); ++i) {
          if (adds[i]>=0) {
            adds[sz] = adds[i];
            tmp[sz++] = i;
          }
        }
        adds.resize(sz);
        tmp.resize(sz);

        // Add contribution to the Jacobian
        ret.nz(adds) = fsens[d][oind].nz(tmp);

        if (symmetric) {
          // Get entries in fsens[d][oind] with nonnegative indices
          tmp.resize(adds2.size());
          sz = 0;
          for (int i=0; i<adds2.size(); ++i) {
            if (adds2[i]>=0) {
              adds2[sz] = adds2[i];
              tmp[sz++] = i;
            }
          }
          adds2.resize(sz);
          tmp.resize(sz);

          // Add contribution to the Jacobian
          ret.nz(adds2) = fsens[d][oind].nz(tmp);
        }
      }

      // Add elements to the Jacobian matrix
      for (int d=0; d<nadir_batch; ++d) {

        // Locate the nonzeros of the adjoint sensitivity matrix
        sparsity_in(iind).find(nzmap);
        asens[d][iind].sparsity().get_nz(nzmap);

        // For all the output nonzeros treated in the sweep
        for (int el = D2.colind(offset_nadir+d); el<D2.colind(offset_nadir+d+1); ++el) {

          // Get the output nonzero
          int r = D2.row(el);

          // Loop over the input nonzeros that influences this output nonzero
          for (int elJ = jsp.colind(r); elJ<jsp.colind(r+1); ++elJ) {

            // Get the input nonzero
            int inz = jsp.row(elJ);

            // Get the corresponding adjoint sensitivity nonzero
            int anz = nzmap[inz];
            if (anz<0) continue;

            // Get the input seed
            ret.nz(elJ) = asens[d][iind].nz(anz);
          }
        }
      }

      // Update direction offsets
      offset_nfdir += nfdir_batch;
      offset_nadir += nadir_batch;
    }

    // Return
    if (verbose()) userOut() << "XFunction::jac end" << std::endl;
    return ret.T();
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>
  ::getGradient(const std::string& name, int iind, int oind, const Dict& opts) {
    // Create expressions for the gradient
    std::vector<MatType> ret_out;
    ret_out.reserve(1+out_.size());
    ret_out.push_back(grad(iind, oind));
    ret_out.insert(ret_out.end(), out_.begin(), out_.end());

    // Return function
    return Function(name, in_, ret_out, opts);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>
  ::getTangent(const std::string& name, int iind, int oind, const Dict& opts) {
    // Create expressions for the gradient
    std::vector<MatType> ret_out;
    ret_out.reserve(1+out_.size());
    ret_out.push_back(tang(iind, oind));
    ret_out.insert(ret_out.end(), out_.begin(), out_.end());

    // Return function
    return Function(name, in_, ret_out, opts);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>
  ::getJacobian(const std::string& name, int iind, int oind, bool compact, bool symmetric,
              const Dict& opts) {
    // Return function expression
    std::vector<MatType> ret_out;
    ret_out.reserve(1+out_.size());
    ret_out.push_back(jac(iind, oind, compact, symmetric));
    ret_out.insert(ret_out.end(), out_.begin(), out_.end());

    // Return function
    return Function(name, in_, ret_out, opts);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>
  ::get_forward_old(const std::string& name, int nfwd, Dict& opts) {
    // Seeds
    std::vector<std::vector<MatType> > fseed = symbolicFwdSeed(nfwd, in_), fsens;

    // Evaluate symbolically
    static_cast<DerivedType*>(this)->evalFwd(fseed, fsens);
    casadi_assert(fsens.size()==fseed.size());

    // Number inputs and outputs
    int num_in = n_in();
    int num_out = n_out();

    // All inputs of the return function
    std::vector<MatType> ret_in;
    ret_in.reserve(num_in + num_out + nfwd*num_in);
    ret_in.insert(ret_in.end(), in_.begin(), in_.end());
    for (int i=0; i<num_out; ++i) {
      std::stringstream ss;
      ss << "dummy_output_" << i;
      ret_in.push_back(MatType::sym(ss.str(), Sparsity(out_.at(i).size())));
    }
    for (int d=0; d<nfwd; ++d)
      ret_in.insert(ret_in.end(), fseed[d].begin(), fseed[d].end());

    // All outputs of the return function
    std::vector<MatType> ret_out;
    ret_out.reserve(num_out*nfwd);
    for (int d=0; d<nfwd; ++d) {
      ret_out.insert(ret_out.end(), fsens[d].begin(), fsens[d].end());
    }

    // Assemble function and return
    return Function(name, ret_in, ret_out, opts);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>
  ::get_reverse_old(const std::string& name, int nadj, Dict& opts) {
    // Seeds
    std::vector<std::vector<MatType> > aseed = symbolicAdjSeed(nadj, out_), asens;

    // Evaluate symbolically
    static_cast<DerivedType*>(this)->evalAdj(aseed, asens);

    // Number inputs and outputs
    int num_in = n_in();
    int num_out = n_out();

    // All inputs of the return function
    std::vector<MatType> ret_in;
    ret_in.reserve(num_in + num_out + nadj*num_out);
    ret_in.insert(ret_in.end(), in_.begin(), in_.end());
    for (int i=0; i<num_out; ++i) {
      std::stringstream ss;
      ss << "dummy_output_" << i;
      ret_in.push_back(MatType::sym(ss.str(), Sparsity(out_.at(i).size())));
    }
    for (int d=0; d<nadj; ++d)
      ret_in.insert(ret_in.end(), aseed[d].begin(), aseed[d].end());

    // All outputs of the return function
    std::vector<MatType> ret_out;
    ret_out.reserve(num_in*nadj);
    for (int d=0; d<nadj; ++d) {
      ret_out.insert(ret_out.end(), asens[d].begin(), asens[d].end());
    }

    // Assemble function and return
    return Function(name, ret_in, ret_out, opts);
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  bool XFunction<DerivedType, MatType, NodeType>
  ::isInput(const std::vector<MatType>& arg) const {
    // Check if arguments matches the input expressions, in which case
    // the output is known to be the output expressions
    const int checking_depth = 2;
    for (int i=0; i<arg.size(); ++i) {
      if (!is_equal(arg[i], in_[i], checking_depth)) {
        return false;
      }
    }
    return true;
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::
  forward_x(const std::vector<MatType>& arg, const std::vector<MatType>& res,
            const std::vector<std::vector<MatType> >& fseed,
            std::vector<std::vector<MatType> >& fsens) {
    // Quick return if no seeds
    if (fseed.empty()) {
      fsens.clear();
      return;
    }

    if (isInput(arg)) {
      // Argument agrees with in_, call evalFwd directly
      static_cast<DerivedType*>(this)->evalFwd(fseed, fsens);
    } else {
      // Need to create a temporary function
      Function f("tmp", arg, res);
      static_cast<DerivedType *>(f.get())->evalFwd(fseed, fsens);
    }
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  void XFunction<DerivedType, MatType, NodeType>::
  reverse_x(const std::vector<MatType>& arg, const std::vector<MatType>& res,
            const std::vector<std::vector<MatType> >& aseed,
            std::vector<std::vector<MatType> >& asens) {
    // Quick return if no seeds
    if (aseed.empty()) {
      asens.clear();
      return;
    }

    if (isInput(arg)) {
      // Argument agrees with in_, call evalAdj directly
      static_cast<DerivedType*>(this)->evalAdj(aseed, asens);
    } else {
      // Need to create a temporary function
      Function f("tmp", arg, res);
      static_cast<DerivedType *>(f.get())->evalAdj(aseed, asens);
    }
  }

  // Helper class
  template<typename MatType>
  class Factory {
  public:

    // All auxiliary outputs
    const Function::AuxOut& aux_;

    // All input and output expressions created so far
    std::map<std::string, MatType> in_, out_;

    // Forward mode directional derivatives
    std::vector<std::string> fwd_in_, fwd_out_;

    // Reverse mode directional derivatives
    std::vector<std::string> adj_in_, adj_out_;

    // Constructor
    Factory(const Function::AuxOut& aux) : aux_(aux) {}

    // Add an input expression
    void add_input(const std::string& s, const MatType& e);

    // Add an output expression
    void add_output(const std::string& s, const MatType& e);

    // Request a factory input
    void request_input(const std::string& s);

    // Request a factory output
    void request_output(const std::string& s);

    // Calculate requested outputs
    void calculate();

    // Retrieve an input
    MatType get_input(const std::string& s);

    // Retrieve an output
    MatType get_output(const std::string& s);

    // Helper function
    static bool has_prefix(const std::string& s);

    // Split prefix
    static std::pair<std::string, std::string> split_prefix(const std::string& s);

    // Check if input exists
    bool has_in(const std::string& s) const { return in_.find(s)!=in_.end();}

    // Check if out exists
    bool has_out(const std::string& s) const { return out_.find(s)!=out_.end();}
  };

  template<typename MatType>
  void Factory<MatType>::
  add_input(const std::string& s, const MatType& e) {
    auto it = in_.insert(make_pair(s, e));
    casadi_assert_message(it.second, "Duplicate input expression \"" + s + "\"");
  }

  template<typename MatType>
  void Factory<MatType>::
  add_output(const std::string& s, const MatType& e) {
    auto it = out_.insert(make_pair(s, e));
    casadi_assert_message(it.second, "Duplicate output expression \"" + s + "\"");
  }

  template<typename MatType>
  void Factory<MatType>::
  request_input(const std::string& s) {
    using namespace std;

    // Quick return if already available
    if (has_in(s)) return;

    // Get prefix
    casadi_assert_message(has_prefix(s), "Cannot process \"" + s + "\"");
    pair<string, string> ss = split_prefix(s);

    if (ss.first=="fwd") {
      // Forward mode directional derivative
      casadi_assert_message(has_in(ss.second), "Cannot process \"" + s + "\"");
      fwd_in_.push_back(ss.second);
    } else if (ss.first=="adj") {
      // Reverse mode directional derivative
      casadi_assert_message(has_out(ss.second), "Cannot process \"" + s + "\"");
      adj_in_.push_back(ss.second);
    }
  }

  template<typename MatType>
  void Factory<MatType>::
  request_output(const std::string& s) {
    using namespace std;

    // Quick return if already available
    if (has_out(s)) return;

    // Get prefix
    casadi_assert_message(has_prefix(s), "Cannot process \"" + s + "\"");
    pair<string, string> ss = split_prefix(s);

    if (ss.first=="fwd") {
      // Forward mode directional derivative
      casadi_assert_message(has_out(ss.second), "Cannot process \"" + s + "\"");
      fwd_out_.push_back(ss.second);
    } else if (ss.first=="adj") {
      // Reverse mode directional derivative
      casadi_assert_message(has_in(ss.second), "Cannot process \"" + s + "\"");
      adj_out_.push_back(ss.second);
    }
  }

  template<typename MatType>
  void Factory<MatType>::calculate() {
    using namespace std;

    // Dual variables
    for (auto&& e : out_) {
      string dual_name = "lam_" + e.first;
      auto it = in_.insert(make_pair(dual_name,
        MatType::sym(dual_name, e.second.sparsity())));
      casadi_assert_message(it.second, "Cannot process \"" + dual_name + "\"");
    }

    // Forward mode directional derivatives
    if (!fwd_out_.empty()) {
      casadi_assert(!fwd_in_.empty());
      vector<MatType> arg, res;
      vector<vector<MatType>> seed(1), sens(1);
      // Inputs and forward mode seeds
      for (const string& s : fwd_in_) {
        arg.push_back(in_[s]);
        string fname = "fwd_" + s;
        seed[0].push_back(MatType::sym(fname, arg.back().sparsity()));
        in_[fname] = seed[0].back();
      }
      // Outputs
      for (const string& s : fwd_out_) {
        res.push_back(out_[s]);
      }
      // Calculate directional derivatives
      Function df("df", arg, res, fwd_in_, fwd_out_);
      df.forward(arg, res, seed, sens, true, false);

      // Get directional derivatives
      for (int i=0; i<fwd_out_.size(); ++i) {
        out_["fwd_" + fwd_out_[i]] = project(sens[0].at(i), res.at(i).sparsity());
      }
    }

    // Reverse mode directional derivatives
    if (!adj_out_.empty()) {
      casadi_assert(!adj_in_.empty());
      vector<MatType> arg, res;
      vector<vector<MatType>> seed(1), sens(1);
      // Inputs
      for (const string& s : adj_in_) {
        arg.push_back(in_[s]);
      }
      // Outputs and reverse mode seeds
      for (const string& s : adj_out_) {
        res.push_back(out_[s]);
        string aname = "adj_" + s;
        seed[0].push_back(MatType::sym(aname, res.back().sparsity()));
        in_[aname] = seed[0].back();
      }
      // Calculate directional derivatives
      Function df("df", arg, res, adj_in_, adj_out_);
      df.reverse(arg, res, seed, sens, true, false);

      // Get directional derivatives
      for (int i=0; i<adj_out_.size(); ++i) {
        out_["adj_" + adj_out_[i]] = project(sens[0].at(i), arg.at(i).sparsity());
      }
    }



  }

  template<typename MatType>
  MatType Factory<MatType>::get_input(const std::string& s) {
    auto it = in_.find(s);
    casadi_assert(it!=in_.end());
    return it->second;
  }

  template<typename MatType>
  MatType Factory<MatType>::get_output(const std::string& s) {
    return MatType();
  }

  template<typename MatType>
  bool Factory<MatType>::
  has_prefix(const std::string& s) {
    return s.find('_') < s.size();
  }

  template<typename MatType>
  std::pair<std::string, std::string> Factory<MatType>::
  split_prefix(const std::string& s) {
    // Get prefix
    casadi_assert(!s.empty());
    size_t pos = s.find('_');
    casadi_assert_message(pos<s.size(), "Cannot process \"" + s + "\"");
    return make_pair(s.substr(0, pos), s.substr(pos+1, std::string::npos));
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  Function XFunction<DerivedType, MatType, NodeType>::
  factory(const std::string& name,
          const std::vector<std::string>& s_in,
          const std::vector<std::string>& s_out,
          const Function::AuxOut& aux,
          const Dict& opts) const {
    using namespace std;

    // Create an expression factory
    Factory<MatType> f(aux);
    for (int i=0; i<in_.size(); ++i) f.add_input(ischeme_[i], in_[i]);
    for (int i=0; i<out_.size(); ++i) f.add_output(oscheme_[i], out_[i]);

    // Specify input and output expressions to be calculated
    for (const string& s : s_in) f.request_input(s);
    for (const string& s : s_out) f.request_output(s);

    // Calculate expressions
    f.calculate();

    // Get input expressions
    vector<MatType> ret_in;
    ret_in.reserve(s_in.size());
    for (const string& s : s_in) ret_in.push_back(f.get_input(s));

    // Get output expressions
    vector<MatType> ret_out;
    ret_out.reserve(s_out.size());
    for (const string& s : s_out) ret_out.push_back(f.get_output(s));

    // Legacy

    // Add linear combinations
    for (auto i : aux) {
      MatType lc = 0;
      for (auto j : i.second) {
        lc += dot(f.in_.at("lam_" + j), f.out_.at(j));
      }
      f.out_[i.first] = lc;
    }

    // List of valid attributes
    const vector<string> all_attr = {"transpose", "triu", "tril", "densify", "sym", "withdiag"};

    // Handle outputs
    for (int i=0; i<ret_out.size(); ++i) {
      MatType& r = ret_out[i];

      // Output name
      string s = s_out[i];

      // Separarate attributes
      vector<string> attr;
      while (true) {
        // Find the first underscore separator
        size_t pos = s.find('_');
        if (pos>=s.size()) break; // No more underscore
        string a = s.substr(0, pos);

        // Try to match with attributes
        auto it = std::find(all_attr.begin(), all_attr.end(), a);
        if (it==all_attr.end()) break; // No more attribute

        // Save attribute and strip from s
        attr.push_back(*it);
        s = s.substr(pos+1, string::npos);
      }

      // Try to locate in list of outputs
      auto it = f.out_.find(s);
      if (it!=f.out_.end()) {
        // Already treated
        r = it->second;
      } else {
        // Must be an operator
        size_t pos = s.find('_');
        casadi_assert_message(pos<s.size(), s_out[i] + " is not an output or operator");
        string op = s.substr(0, pos);
        s = s.substr(pos+1);

        // Handle different types of operators
        if (op=="grad" || op=="jac" || op=="hess") {
          // Output
          pos = s.find('_');
          casadi_assert_message(pos<s.size(), s_out[i] + " is ill-posed");
          string res = s.substr(0, pos);
          auto res_i = f.out_.find(res);
          casadi_assert_message(res_i!=f.out_.end(),
                                "Unrecognized output " + res + " in " + s_out[i]);
          s = s.substr(pos+1);

          // Input
          string arg;
          if (op=="hess") {
            pos = s.find('_');
            casadi_assert_message(pos<s.size(), s_out[i] + " is ill-posed");
            arg = s.substr(0, pos);
            s = s.substr(pos+1);
            casadi_assert_message(s==arg, "Mixed Hessian terms not supported");
          } else {
            arg = s;
          }
          auto arg_i = f.in_.find(arg);
          casadi_assert_message(arg_i!=f.in_.end(),
                                "Unrecognized input " + arg + " in " + s_out[i]);

          // Calculate gradient or Jacobian
          if (op=="jac") {
            r = MatType::jacobian(res_i->second, arg_i->second);
          } else if (op=="grad") {
            r = project(MatType::gradient(res_i->second, arg_i->second), arg_i->second.sparsity());
          } else {
            casadi_assert(op=="hess");
            r = triu(MatType::hessian(res_i->second, arg_i->second));
          }
        } else {
          casadi_error("Unknown operator: " + op);
        }
      }

      // Apply attributes (starting from the right-most one)
      for (auto a=attr.rbegin(); a!=attr.rend(); ++a) {
        if (*a=="transpose") {
          r = r = r.T();
        } else if (*a=="triu") {
          r = triu(r);
        } else if (*a=="tril") {
          r = tril(r);
        } else if (*a=="densify") {
          r = densify(r);
        } else if (*a=="sym") {
          r = triu2symm(r);
        } else if (*a=="withdiag") {
          r = project(r, r.sparsity() + Sparsity::diag(r.size1()));
        } else {
          casadi_assert(0);
        }
      }
    }

    // Create function and return
    Function ret(name, ret_in, ret_out, s_in, s_out, opts);
    if (ret.has_free()) {
      stringstream ss;
      ss << "Cannot generate " << name << " as the expressions contain free variables: ";
      if (ret.is_a("sxfunction")) {
        ss << ret.free_sx();
      } else {
        casadi_assert(ret.is_a("mxfunction"));
        ss << ret.free_mx();
      }
      casadi_error(ss.str());
    }
    return ret;
  }

  template<typename DerivedType, typename MatType, typename NodeType>
  std::vector<bool> XFunction<DerivedType, MatType, NodeType>::
  nl_var(const std::string& s_in, const std::vector<std::string>& s_out) const {
    using namespace std;

    // Input arguments
    auto it = find(ischeme_.begin(), ischeme_.end(), s_in);
    casadi_assert(it!=ischeme_.end());
    MatType arg = in_.at(it-ischeme_.begin());

    // Output arguments
    vector<MatType> res;
    for (auto&& s : s_out) {
      it = find(oscheme_.begin(), oscheme_.end(), s);
      casadi_assert(it!=oscheme_.end());
      res.push_back(out_.at(it-oscheme_.begin()));
    }

    // Extract variables entering nonlinearly
    return MatType::nl_var(veccat(res), arg);
  }

} // namespace casadi
/// \endcond

#endif // CASADI_X_FUNCTION_INTERNAL_HPP

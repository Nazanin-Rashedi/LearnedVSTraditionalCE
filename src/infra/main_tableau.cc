#include "glob_infra_standard_includes.hh"
#include "matrix.hh"
#include "vector_ops.hh"
#include "double_vec_ops.hh"
#include "FourierMotzkinBasic.hh"

// #include "vp_ops.hh"


class Tableau {
  public:
    Tableau();
  public:
    void set_matrix(const Matrix& A);
    // Austausch von e_i durch a_j, A(i,j) ist pivotelement != 0
    // returns false if A(i,j) == 0
    bool exchange(const uint i, const uint j); 
  public:
    inline const uint noRows() const { return _A.noRows(); }
    inline const uint noCols() const { return _A.noCols(); }
    inline const Matrix& get_A() const { return _A; }
    inline       Matrix& get_A_nc() { return _A; }
    inline double get_A(const uint i, const uint j) const { return _A(i,j); }
  public:
    std::ostream& print(std::ostream& os, const int aWidth) const;
  private:
    Matrix    _A;
    int_vt    _hdr_col; // indices positive: a_i  negative: e_i
    int_vt    _hdr_row; // indices positive: e_i, negative: a_i
    double_vt _kellerzeile;
};

Tableau::Tableau() : _A(), _hdr_col(), _hdr_row(), _kellerzeile() {
}

void
Tableau::set_matrix(const Matrix& A) {
  _A = A;
  const uint m = _A.noRows();
  const uint n = _A.noCols();
  _hdr_row.resize(m);
  _hdr_col.resize(n);
  _kellerzeile.resize(n);

  for(uint i = 0; i < m; ++i) {
    _hdr_row[i] = i + 1;
  }
  for(uint j = 0; j < n; ++j) {
    _hdr_col[j] = j + 1;
  }
}

bool
Tableau::exchange(const uint ii, const uint jj) {
  const uint m = _A.noRows();
  const uint n = _A.noCols();
  
  if(0 == _A(ii,jj)) {
    return false;
  }

  double lPiv = (((double) 1.0) / _A(ii,jj)); // reciprocal of pivot

  // berechne kellerzeile (1 in Nef p54), see also PosSol/lp.tex

  for(uint j = 0; j < n; ++j) {
    if(0 == _A(ii,j)) {
      _kellerzeile[j] = 0; // um storende -0 zu vermeiden
    } else {
      _kellerzeile[j] = -(_A(ii,j) * lPiv);
    }
  }

  // copy kellerzeile to row exchanged
  for(uint j = 0; j < n; ++j) {
    if(j == jj) {
      _A(ii,j) = lPiv;
    } else {
      _A(ii,j) = _kellerzeile[j];
    }
  }

  // block 2
  // adjust all non-pivot columns in all rows
  for(uint i = 0; i < m; ++i) {
    if(i == ii) {
      continue;
    }
    for(uint j = 0; j < n; ++j) {
      if(j == jj) {
        continue;
      }
      _A(i,j) += _kellerzeile[j] * _A(i,jj);
    }
  }

  // block 1 (vertauscht gegenueber Nef p64 da _A(i,jj) veraendert wird)
  // adjust pivot column
  for(uint i = 0; i < m; ++i) {
    if(i == ii) {
      continue;
    }
    _A(i,jj) *= lPiv;
  }

  // adjust header
  _hdr_row[ii] = -(jj+1);
  _hdr_col[jj] = -(ii+1);
  return 0;
}

std::ostream&
Tableau::print(std::ostream& os, const int aWidth) const {
  const uint m = noRows();
  const uint n = noCols(); 

  const int lWidth = std::max(6, aWidth);
  os << "        ";
  for(uint j = 0; j < noCols(); ++j) {
    if(0 < _hdr_col[j]) {
      os << ' ' << "a_{" << std::setw(2) << std::left << _hdr_col[j] << '}';
    } else {
      os << ' ' << "e_{" << std::setw(2) << std::left << (-_hdr_col[j]) << '}';
    }
  }
  os << std::endl;
  for(uint i = 0; i < m; ++i) {
    if(0 < _hdr_row[i]) {
      os << " e_{" << std::setw(2) << std::left <<   _hdr_row[i]  << '}';
    } else{
      os << " a_{" << std::setw(2) << std::left << (-_hdr_row[i]) << '}';
    }
    for(uint j = 0; j < n; ++j) {
      os << ' ' << std::setw(lWidth) << std::right << _A(i,j);
    }
    os << std::endl;
  }
  os << "---" << std::endl;
  os << "       ";
  for(uint j = 0; j < n; ++j) {
    os << ' ' << std::setw(lWidth) << std::right << _kellerzeile[j];
  }
  os << std::endl;
  return os;
}

void
test0() {
  Matrix A(2,3);
  A(0,0) =  2; A(0,1) = 1; A(0,2) = -1;
  A(1,0) = -1; A(1,1) = 2; A(1,2) =  3;

  Tableau lTab;
  lTab.set_matrix(A);
  lTab.print(std::cout, 4) << std::endl;

  lTab.exchange(0,0);
  std::cout << "after exchange(0,0)" << std::endl;
  lTab.print(std::cout, 4) << std::endl;

  lTab.exchange(1,2);
  std::cout << "after exchange(1,2)" << std::endl;
  lTab.print(std::cout, 4) << std::endl;
}

void
test1(const uint n) {
  Matrix A(n,n);
  for(uint i = 0; i < n; ++i) {
    for(uint j = 0; j < n; ++j) {
       A(i,j) = ((i | j) == j);
    }
  }
  std::cout << "A = " << std::endl;
  A.print(std::cout, 3) << std::endl;

  Tableau lTab;
  lTab.set_matrix(A);

  for(uint i = 0; i < n; ++i) {
    lTab.exchange(i,i);
    std::cout << "after exchange(" << i << ',' << i << ')' << std::endl;
    lTab.print(std::cout, 4) << std::endl;
  }
}

void
run_ks(const uint_vt& aKsPat, const double_vt& aKsVal) {
  assert(aKsPat.size() == aKsVal.size());
  std::cout << "run_ks:" << std::endl
            << "   pat = " << std::setw(3) << aKsPat << std::endl
            << "   val = " << std::setw(3) << aKsVal << std::endl
            << std::endl;

  const uint n = 4;
  const uint m = aKsPat.size();
  const uint N = (1 << n);

  Matrix A(m, N + 1);
  for(uint i = 0; i < m; ++i) {
    const uint ii = aKsPat[i];
    for(uint j = 0; j < N; ++j) {
       A(i,j) =  ((ii | j) == j);
    }
  }
  for(uint i = 0; i < m; ++i) {
    A(i,N) = -aKsVal[i];
  }
  std::cout << "A = " << std::endl;
  A.print(std::cout, 3) << std::endl;

  Tableau lTab;
  lTab.set_matrix(A);

  uint_vt lColIdx; // column indices for which exchange was taken
  for(uint i = 0; i < m; ++i) {
    for(uint j = 0; j < N; ++j) {
      if(0 < A(i,j)) {
         lTab.exchange(i,j);
         std::cout << "after exchange(" << i << ',' << j << ')' << std::endl;
         lTab.print(std::cout, 4) << std::endl;
         lColIdx.push_back(j);
         break;
      }
    }
  }
  std::cout << "lColIdx = " << lColIdx << std::endl;
  // extract matrices B, C, and vector D
  Matrix B(m, lColIdx.size());
  Matrix C(m, N - lColIdx.size());
  double_vt D(m);

  uint k = 0; // index into lColIdx
  uint jjB = 0; // column index of B
  uint jjC = 0; // column index of C
  for(uint j = 0; j < N; ++j) {
    if(j == lColIdx[k]) {
      for(uint i = 0; i < m; ++i) {
        B(i, jjB) = lTab.get_A(i,j);
      }
      ++jjB;
      ++k;
    } else {
      for(uint i = 0; i < m; ++i) {
        C(i, jjC) = lTab.get_A(i,j);
      }
      ++jjC;
    }
  }
  for(uint i = 0; i < m; ++i) {
    D[i] = lTab.get_A(i,N);
  }

  std::cout << "B = " << std::endl;
  B.printLatex(std::cout, 3, 0) << std::endl;

  std::cout << "C = " << std::endl;
  C.printLatex(std::cout, 3, 0) << std::endl;

  std::cout << "D = " << D << std::endl;

}

void
run_ks_lp(const uint_vt& aKsPat, const double_vt& aKsVal) {
  assert(aKsPat.size() == aKsVal.size());
  std::cout << "run_ks_lp:" << std::endl
            << "   pat = " << std::setw(3) << aKsPat << std::endl
            << "   val = " << std::setw(3) << aKsVal << std::endl
            << std::endl;

  const uint n = 4;
  const uint m = aKsPat.size();
  const uint N = (1 << n);

  Matrix A(m + 1, N + 2); // m + 1 wg. zielfunktion, N + 2 wg. beta und chi (-beta zu anfang)
  // fill A(0..m,0..N) with design matrix
  for(uint i = 0; i < m; ++i) {
    const uint ii = aKsPat[i];
    for(uint j = 0; j < N; ++j) {
       A(i,j) =  ((ii | j) == j);
    }
  }

  // fill in tau (in Nef) (= beta), chi = -beta
  for(uint i = 0; i < m; ++i) {
    A(i,N)   =  aKsVal[i];
    A(i,N+1) = -aKsVal[i];
  }

  // fill in zielfunktion: x_{N-1} max, -x_{N-1} min
  A(m,N-1) = -1;

  std::cout << "A = " << std::endl;
  A.print(std::cout, 3) << std::endl;

  Tableau lTab;
  lTab.set_matrix(A);

  int    lIdxOld = -1;
  int    lIdx    = 0;
  double lMax    = lTab.get_A(0,N+1);
  for(uint lRound = 0; lRound < 5; ++lRound) {
    lIdx = (lIdxOld == 0 ? 1 : 0);
    lMax = lTab.get_A(lIdx,N+1);
    for(uint i = 0; i < m; ++i) {
      if((int) i == lIdxOld) {
        continue;
      }
      if(lTab.get_A(i,N+1) > lMax) {
        lMax = lTab.get_A(i,N+1);
        lIdx = i;
      }
    }
    lTab.exchange(lIdx, N-1);
    std::cout << "after exchange(" << lIdx << ',' << (N-1) << ')' << std::endl;
    // lTab.print(std::cout, 4) << std::endl;
    // std::cout << "T.A = " << std::endl;
    lTab.get_A().print(std::cout, 3) << std::endl;
    lIdxOld = lIdx;
  }
}

void
run_ks_lp_2(const uint_vt& aKsPat, const double_vt& aKsVal) {
  assert(aKsPat.size() == aKsVal.size());
  std::cout << "run_ks_lp_2:" << std::endl
            << "   pat = " << std::setw(3) << aKsPat << std::endl
            << "   val = " << std::setw(3) << aKsVal << std::endl
            << std::endl;

  const uint n = 4;
  const uint m = aKsPat.size();
  const uint N = (1 << n);

  Matrix A(m + 1, N + 2); // m + 1 wg. zielfunktion, N + 2 wg. beta und chi (-beta zu anfang)
  // fill A(0..m,0..N) with design matrix
  for(uint i = 0; i < m; ++i) {
    const uint ii = aKsPat[i];
    for(uint j = 0; j < N; ++j) {
       A(i,j) =  ((ii | j) == j);
    }
  }

  // fill in tau (in Nef) (= beta), chi = -beta
  for(uint i = 0; i < m; ++i) {
    A(i,N)   =  aKsVal[i];
    A(i,N+1) = -aKsVal[i];
  }
  
  // fill in zielfunktion: x_{N-1} max, -x_{N-1} min
  A(m,N-1) = -1;
  
  std::cout << "A = " << std::endl;
  A.print(std::cout, 3) << std::endl;
  
  Tableau lTab;
  lTab.set_matrix(A);
  
  for(uint lPivIdx = 0; lPivIdx < m; ++lPivIdx) {
    lTab.exchange(lPivIdx, lPivIdx);
    std::cout << "after exchange(" << lPivIdx << ',' << lPivIdx << ')' << std::endl;
    // lTab.print(std::cout, 4) << std::endl;
    // std::cout << "T.A = " << std::endl;
    lTab.get_A().print(std::cout, 3) << std::endl;
  }
}

#include "LpGr.cc"

/*
bool
run_ks_lp_3(const uint_vt& aKsPat, const double_vt& aKsVal) {
  assert(aKsPat.size() == aKsVal.size());
  std::cout << "run_ks_lp_3:" << std::endl
            << "   pat = " << std::setw(3) << aKsPat << std::endl
            << "   val = " << std::setw(3) << aKsVal << std::endl
            << std::endl;

  const uint n = 4;
  const uint m = aKsPat.size();
  const uint N = (1 << n);

  Matrix A(m,N); 

  // fill A(0..m,0..N) with design matrix
  for(uint i = 0; i < m; ++i) {
    const uint ii = aKsPat[i];
    for(uint j = 0; j < N; ++j) {
       A(i,  j) =  ((ii | j) == j);  // >= beta[i]
    }
  }

  // slackvariablen
  // for(uint j = 0; j < m; ++j) {
  //   A(j, N + j) = 1;
  // }


  std::cout << "A = " << std::endl;
  A.print(std::cout, 3) << std::endl;

  // uint_vt lColIdxBase(m);  // Spaltenindexvektor der Basis
  // uint_vt lColIdxOther(N); // Spaltenindexvektorn der restlichen Spalten
 
  // initiale Basis ist Spalte N,...N+m
  // for(uint j = 0; j < m; ++j) {
  //   lColIdxBase[j] = N+j;
  // }

  // for(uint j = 0; j < N; ++j) {
  //   lColIdxOther[j] = j;
  // } 

  // std::cout << "B = " << lColIdxBase << std::endl;
  // std::cout << "N = " << lColIdxOther << std::endl;

  // Basisloesung
  // x_N = 0, x_B = A_B^{-1} b
  double_vt b(m);
  for(uint j = 0; j < m; ++j) {
     b[j] = aKsVal[j];
  }

  std::cout << "b = " << b << std::endl;


  // zielfunktion
  double_vt c(N); // \ol{c}^t = c_N^t - c_B^t A_B^{-1} A_N
  uint_vt   l1A(N);  // remember the number of '1' in every column (= column sum = \vec{1} A
  // da im momeent c_B^t = 0 gilt \ol{c}^t = c_N^t
  for(uint j = 0; j < N; ++j) {
    uint lNoOne = 0;
    for(uint i = 0; i < m; ++i) {
      const uint ii = aKsPat[i];
      lNoOne += ((ii | j) == j);
    }
    c[j]   = lNoOne;
    l1A[j] = lNoOne;
  }
  std::cout << "c = " << c << std::endl;

  // zielfunktionswert
  double c0 = 0; 
  std::cout << "c0 = " << c0 << std::endl;
 
  // \ol{A} = A_B^{-1} A_N. here: A_B = A_B^{-1} = I

  // indexmengen
  uint_vt lBidx(m); // indexmenge der basisloesung
  uint_vt lNidx(N); // indexmenge der uebrigen spaltenvektoren
  for(uint i = 0; i < m; ++i) {
    lBidx[i] = N + i;
  }
  for(uint j = 0; j < N; ++j) {
    lNidx[j] = j;
  }
  std::cout << "  B = " << lBidx << std::endl;
  std::cout << "  N = " << lNidx << std::endl;

  double_vt v(N);  // Hilfsvektor
  uint lRound = 0;
  for(lRound = 0; lRound < 1000; ++lRound) {
    std::cout << "  Round " << lRound << std::endl;

    // optimal?
    const double lEps = 1E-10;
    bool lOptimal = true;
    for(uint j = 0; j < N; ++j) {
      if(lEps <= c[j]) {
        lOptimal = false;
        break;
      }
    }
    if(lOptimal) {
      break;
    }

    // suche austauschspalte
    uint s = 0;
    for(uint j = 0; j < N; ++j) {
      if(0 < c[j]) {
        s = j;
        break;
      }
    }
    std::cout << "    s = " << s << "  (pivot column)" << std::endl;

    // unbeschraenkt (brauchen wir eigentlich nicht)
    bool lBeschraenkt = false;
    for(uint i = 0; i < m; ++i) {
      if(0 < A(i, s)) {
        lBeschraenkt = true;
        break;
      }
    }
    if(!lBeschraenkt) {
      std::cout << "    unbeschraenkt" << std::endl;
    }

    double lLambda0 = std::numeric_limits<double>::max();
    uint r   = 0;
    for(uint i = 0; i < m; ++i) {
      if(0 < A(i,s)) {
        const double lQuot = b[i] / A(i,s);
        if(lQuot < lLambda0) { // < is alternative
          lLambda0 = lQuot;
          r = i;
        }
      }
    }
    std::cout << "    r = " << r 
              << " , lambda_0 = " << lLambda0 
              << std::endl;
  
    // NEUBERECHNUNGEN
    const double x = ((double) 1.0) / A(r,s);
    std::cout << "    x = " << x << std::endl;

    // berechne v
    for(uint j = 0; j < N; ++j) {
      v[j] = x * A(r,j);
    }

    std::cout << "    v = " << v << std::endl;

    // Neuberechnung c
    const double w = c[s];
    for(uint j = 0; j < N; ++j) {
      if(j == s) { continue; }
      c[j] = c[j] - v[j] * w;
    }
    c[s] = - x * w;
    c0 = c0 + x * b[r] * w;

    std::cout << "    c  = " << c << std::endl;
    std::cout << "    c0 = " << c0 << std::endl;

    // Neuberechnung b
    const double z = x * b[r];
    for(uint i = 0; i < m; ++i) {
      if(i == r) { continue; }
      b[i] = b[i] - z * A(i,s);
    }
    b[r] = z;
    std::cout << "    b = " << b << std::endl;

    // Neuberechnung A
    for(uint i = 0; i < m; ++i) {
      if(i == r) { continue; }
      const double y = A(i,s);
      if(0 == y) { continue; }
      for(uint j = 0; j < N; ++j) {
        if(j == s) { continue; }
        A(i,j) = A(i,j) - v[j] * y;
      }
    }
    for(uint j = 0; j < N; ++j) {
      if(j == s) { continue; }
      A(r,j) = v[j];
    }
    for(uint i = 0; i < m; ++i) {
      if(i == r) { continue; }
      A(i,s) = - x * A(i,s);
    }
    A(r,s) = x;

    // std::cout << "    A = " << std::endl;
    // A.print(std::cout, 3, 6, Matrix::kAscii);

    // Neuberechnung N, B
    std::swap(lBidx[r], lNidx[s]);
    std::cout << "    B = " << lBidx << std::endl;
    std::cout << "    N = " << lNidx << std::endl;
  }

  double lSumKs = 0; // vec{1} b  fuer stopkriterium/erfolgskriterium
  for(uint i = 0; i < m; ++i) {
    lSumKs += aKsVal[i];
  }

  std::cout << "run_ks_lp_3: fin: c0 = " << c0 << std::endl;
  std::cout << "   b = " << b << std::endl;
  std::cout << "  1A = " << l1A << std::endl;
  std::cout << "  1b = " << lSumKs << "  (here: original b = known sels)" << std::endl;

  double lSum2 = 0;
  for(uint j = 0; j < m; ++j) {
    if(lBidx[j] < N) {
      lSum2 += l1A[lBidx[j]] * b[j];
    } else {
      // lSum2 += b[j];
    }
  }

  std::cout << "  sum(ksval) = " << lSumKs
            << "  =?= " << lSum2 << " ; c0 = " << c0
            << "; diff = " << (lSumKs - c0)
            << "; #R = " << lRound
            << std::endl;

  bool lSolvable = (c0 == lSumKs);
  std::cout << "solvable: " << lSolvable << std::endl;

  if(!lSolvable) {
    return false;
  }

  // analyze case \vec{1} A x = \vec{1} b
  /// not necessary here

  return lSolvable;
}
*/

std::ostream&
print_lp(std::ostream& os, const Matrix& A) {
  const uint m = A.noRows();
  const uint N = A.noCols() - 1;
  for(uint i = 0; i < m; ++i) {
    if(0 != A(i,0)) {
      if(1 == A(i,0)) {
        os << " x_0";
      } else {
        os << A(i,0) << " x_0";
      }
    }
    for(uint j = 1; j < N; ++j) {
      if(0 <  A(i,j)) {
        os << " + ";
        if(1 == A(i,j)) {
          os <<  " x_" << j;
        } else {
          os <<  A(i,j) << " x_" << j;
        } 
      } else
      if(0 > A(i,j)) {
        if(-1 == A(i,j)) {
          os <<  " -x_" << j;
        } else {
          os << " - " << -A(i,j) << " x_" << j;
        } 
      }
    }
    os << " = " << A(i,N) << " ;" << std::endl;
  }
  return os;
}


void
sum_sum(const uint_vt& aKsPat, const double_vt& aKsVal, const uint n) {
  assert(aKsPat.size() == aKsVal.size());
  std::cout << "sum_sum:" << std::endl
            << "   pat = " << std::setw(3) << aKsPat << std::endl
            << "   val = " << std::setw(3) << aKsVal << std::endl
            << std::endl;

  const uint m = aKsPat.size();
  const uint N = (1 << n);

  Matrix A(m,N+1); 

  // fill A(0..m,0..N) with design matrix
  for(uint i = 0; i < m; ++i) {
    const uint ii = aKsPat[i];
    for(uint j = 0; j < N; ++j) {
       A(i,  j) =  ((ii | j) == j);  // >= beta[i]
    }
  }
  // copy aKsVal into last column of A
  for(uint i = 0; i < m; ++i) {
    A(i, N) = aKsVal[i];
  }

  std::cout << "after init:" << std::endl;

  std::cout << "A = " << std::endl;
  A.printWithRowColSum(std::cout, 3) << std::endl;

  Matrix B(A);
  B.convertToHermiteNormalFormUUT();
  std::cout << "herm(A) = " << std::endl;
  B.printWithRowColSum(std::cout, 3) << std::endl;


  if(false) { 
    FourierMotzkinBasic lFm(false);
    lFm.initEq(B);
    std::cout << "FourierMotzkin after init:" << std::endl;
    lFm.print(std::cout, 3) << std::endl;
    lFm.run_elimination();

    std::cout << "  totmem = " << lFm.total_memory() << std::endl;
  }

/*
  uint lNoNP = 0;
  do {
    const uint lMinK = lFm.min_neg_pos_col(&lNoNP);
    std::cout << "lMinK = " << lMinK << ", lMinV = " << lNoNP << std::endl;
    if(lFm.no_vars() <= lMinK) { break; }
    // if(2 < lNoNP) { break; }
    if(!lFm.is_consistent()) { break; }
    if(0 >= lFm.no_vars_left()) { break; }
    lFm.eliminate_var(lMinK);
    std::cout << "FourierMotzkin after elim_var(" << lMinK << "):" << std::endl;
    lFm.print(std::cout, 3) << std::endl;
    
  } while(true);

  std::cout << "  consistent         : " << lFm.is_consistent() << std::endl;
  std::cout << "  all rows consistent: " << lFm.all_rows_consistent() << std::endl;
  std::cout << "  vars eliminated    : " << lFm.no_vars_eliminated() << std::endl;
  std::cout << "  vars left          : " << lFm.no_vars_left() << std::endl;
*/

  
  int s = N-1;
  for(int r = m-1; 0 <= r; --r) {
    while(1 != A(r,s)) {
      --s;
    }
    if(0 > s) {
      break;
    }
    if(1 == A(r,s)) {
      for(int i = 0; i < r; ++i) {
        if(1 == A(i,s)) {
          for(uint j = 0; j <= N; ++j) {
            A(i,j) -= A(r,j); // subtract two rows
          }
        }
      }
    }
    // std::cout << "A = " << std::endl;
    // A.print(std::cout, 3) << std::endl;
  }

  for(uint i = 0; i < m; ++i) {
    if(0 > A(i,N)) {
     for(uint j = 0; j <= N; ++j) {
        if(0 != A(i,j)) {
          A(i,j) *= -1;
        }
     }
   }
  }

  std::cout << "after phase I:" << std::endl;

  std::cout << "A = " << std::endl;
  A.printWithRowColSum(std::cout, 3) << std::endl;


  bool lChange = true;
  uint lCount = 0;
  while(lChange) {
    ++lCount;
    lChange = false;
    for(int r = m - 1; 0 < r; --r) {
      for(int i = 0; i < r; ++i) {
        if(0 < A(r,N) && A(r,N) <= A(i,N)) {
          // subtract the two
          lChange = true;
          for(uint j = 0; j <= N; ++j) {
            A(i,j) -= A(r,j);
          }
        }
      }
    }
  }

  std::cout << "after phase II:" << std::endl;
  std::cout << "A = " << std::endl;
  A.printWithRowColSum(std::cout, 3) << std::endl;
  std::cout << "count = " << lCount << std::endl;

  // replace '-1' in leading columns by '+1'
  for(uint i = 0; i < m; ++i) {
    if(0 == A(i,N)) {
      for(uint j = i; j < N; ++j) {
        if(0 != A(i,j)) {
          if(-1 == A(i,j)) {
            for(uint k = j; k < N; ++k) {
              if(0 != A(i,k)) {
                A(i,k) = -A(i,k);
              }
            }
          }
          break;
        }
      }
    }
  }

  std::cout << "after sign correction" << std::endl;
  std::cout << "A = " << std::endl;
  A.printWithRowColSum(std::cout, 3) << std::endl;

  // print_lp(std::cout, A);

  if(false) { 
    FourierMotzkinBasic lFm(true);
    lFm.initEq(A);
    std::cout << "FourierMotzkin after init:" << std::endl;
    lFm.print(std::cout, 3) << std::endl;
    lFm.run_elimination();

    std::cout << "  totmem = " << lFm.total_memory() << std::endl;
  }

}

void
test2() {
  uint_vt   lKsPat = {  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = { 75, 41, 38, 12, 38, 13, 17, 37, 14, 14, 18 };

  run_ks(lKsPat, lKsVal);

}

void
test3() {
  uint_vt   lKsPat = {  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };

  run_ks(lKsPat, lKsVal);
}

void
test4() {
  uint_vt   lKsPat = {  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = { 75, 41, 38, 12, 38, 13, 17, 37, 14, 14, 18 };

  std::cout << "test4: inconsistent example:" << std::endl;

  // run_ks_lp(lKsPat, lKsVal);
  // run_ks_lp_2(lKsPat, lKsVal);
  // run_ks_lp_3(lKsPat, lKsVal);
  sum_sum(lKsPat, lKsVal, 4);
}

void
test5() {   
  uint_vt   lKsPat = {   0,  1,  2, 3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = {  44, 17, 20, 3, 22, 11, 10, 40, 17, 16, 19 };
  
  std::cout << "test5: consistent example:" << std::endl;
  // run_ks_lp(lKsPat, lKsVal);
  // run_ks_lp_2(lKsPat, lKsVal);
  // run_ks_lp_3(lKsPat, lKsVal);
  sum_sum(lKsPat, lKsVal, 4);
}

void
test6a() {
  std::cout << "test6a: consistent example:" << std::endl;
  uint_vt   lKsPat = {  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = { 75, 49, 46, 26, 42, 24, 16, 44, 26, 26, 19 };

  // const bool lRes = run_ks_lp_3(lKsPat, lKsVal);
  // std::cout << "exam6a: " << lRes << std::endl;

  sum_sum(lKsPat, lKsVal, 4);
}

void
test6b() {
  std::cout << "test6b: inconsistent example:" << std::endl;
  uint_vt   lKsPat = {  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12 };
  double_vt lKsVal = { 75, 50, 47, 26, 43, 24, 16, 45, 26, 26, 19 };

  sum_sum(lKsPat, lKsVal, 4);
}


void
test7a() {
  std::cout << "test7a: consistent example:" << std::endl;
  uint_vt   lKsPat = {   0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12, 16, 17, 18, 20, 24 };
  double_vt lKsVal = { 119, 55, 55, 15, 57, 24, 27, 74, 31, 30, 37, 77, 36, 33, 33, 32 };

  sum_sum(lKsPat, lKsVal, 5);
}

void
test7b() {
  std::cout << "test6b: inconsistent example:" << std::endl;
  uint_vt   lKsPat = {   0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 12, 16, 17, 18, 20, 24 };
  double_vt lKsVal = { 119, 56, 56, 15, 58, 24, 27, 75, 31, 30, 37, 78, 36, 33, 33, 32 };

  sum_sum(lKsPat, lKsVal, 5);
}

void
test_nef_exam1() {
  Matrix A(4,4);
  A(0,0) =  1; A(0,1) = -1; A(0,2) =  3; A(0,3) =  -3;
  A(1,0) =  0; A(1,1) = -1; A(1,2) =  5; A(1,3) =  -5;
  A(2,0) = -2; A(2,1) = -1; A(2,2) = 20; A(2,3) = -20;
  A(3,0) = -1; A(3,1) = -1; A(3,2) =  0; A(3,3) =  0;

  Tableau lTab;
  lTab.set_matrix(A);
  std::cout << "A = " << std::endl;
  lTab.get_A().print(std::cout, 3) << std::endl;

  uint ii = 0;
  uint jj = 1;
  lTab.exchange(ii, jj);
  std::cout << "after exchange(" << ii << ',' << jj << ')' << std::endl;
  lTab.get_A().print(std::cout, 8) << std::endl;

  ii = 1;
  jj = 0;
  lTab.exchange(ii, jj);
  std::cout << "after exchange(" << ii << ',' << jj << ')' << std::endl;
  lTab.get_A().print(std::cout, 8) << std::endl;

  ii = 2;
  jj = 1;
  lTab.exchange(ii, jj);
  std::cout << "after exchange(" << ii << ',' << jj << ')' << std::endl;
  lTab.get_A().print(std::cout, 8) << std::endl;
  
}


int
main() {
  // test0();
  // test1(4);
  // test1(8);
  // test2();
  // test3();
  // test_nef_exam1();
  test4();
  test5();
  test6a();
  test6b();
  test7a(); // too big
  test7b(); // too big
  return 0;
}


#include <iostream>
#include <iomanip>
#include <vector>

#include "bitvectorsmall.hh"
#include "bit_subsets.hh"
#include "bit_intrinsics.hh"
#include "matrix.hh"
#include "gmsvd.hh"
#include "double_vec_ops.hh"
#include "measure.hh"
#include "tmath.hh"

typedef unsigned int uint;
typedef std::vector<uint> uint_vt;
typedef std::vector<double> double_vt;

template<typename Titem>
std::ostream&
operator<<(std::ostream& os, const std::vector<Titem>& x) {
  os << '[';
  for(uint i = 0; i < x.size(); ++i) {
    os << ' ' << x[i];
  }
  os << " ] ";
  return os;
}

// z is number of boolean factors
// n = 2^z
// full design matrix for selectivity estimation problem

enum format_t {
  kPrint    = 1,
  kMatlab   = 2,
  kLatex    = 4
};

void
fFillVector(double* x, const uint n) {
  for(uint i = 0; i < n; ++i) {
    x[i] = fabs(sin(rand()));
  }
}

void
fSqrtVector(double* y, const double* x, const uint n) {
  for(uint i = 0; i < n; ++i) {
    y[i] = sqrt(x[i]);
  }
}

void
fInvVector(double* y, const double* x, const uint n) {
  for(uint i = 0; i < n; ++i) {
    if(1E-10 < fabs(x[i])) {
      y[i] = (1.0 / x[i]);
    } else {
      y[i] = 0;
    }
  }
}


void
fGetFullDesignMatrix(Matrix& A, const uint z) {
  const uint n = (1 << z);
  const uint lMask = ((1 << z) - 1);

  A.resize(n,n);
  A.setNull();
  for(uint i = 0; i < n; ++i) {
    uint ci = ~i & lMask; // complement of i
    for(BvAllSubsets<uint> lIter(ci); lIter.isValid(); ++lIter) {
       uint lIdx = (i | (*lIter));
       A(i,lIdx) = 1.0;
    }
  }
}

void
fGetElimMatrix(Matrix& A, const uint_vt& aIdxVec, const uint n) {
  A.resize(n, n);
  for(uint i = 0; i < aIdxVec.size(); ++i) {
    const uint lIdx = aIdxVec[i];
    A(lIdx, lIdx) = 1;
  }
}

void
fGetElimMatrix(Matrix& A, const uint_vt& aIdxVec, const uint m, const uint n) {
  A.resize(m,n);
  for(uint i = 0; i < aIdxVec.size(); ++i) {
    const uint lIdx = aIdxVec[i];
    A(i, lIdx) = 1;
  }
}

void
fPrintAllFormats(const Matrix& A, 
                 const uint  aFormats, 
                 const char* aMatrixName, 
                 const uint  aWidth,
                 const bool  aWithNorm = false) {
  if(0 != (aFormats & kPrint)) {
    std::cout << aMatrixName << " (" << A.noRows() << 'x' << A.noCols() << "): " << std::endl;
    A.print(std::cout, aWidth);
    std::cout << std::endl;
  }

  if(0 != (aFormats & kMatlab)) {
    std::cout << "matlab:" << std::endl;
    std::cout << aMatrixName << " = ";
    A.printMatlab(std::cout, aWidth);
    std::cout << ";" << std::endl;
    std::cout << std::endl;
  }

  if(0 != (aFormats & kLatex)) {
    std::cout << "latex:" << std::endl;
    std::cout << "\\begin{pmatrix}" << std::endl;
    for(uint i = 0; i < A.noRows(); ++i) {
      for(uint j = 0; j < A.noCols(); ++j) {
        std::cout << A(i,j);
        if((j + 1) < A.noCols()) {
          std::cout << " & ";
        }
      }
      if((i + 1) < A.noRows()) {
        std::cout << " \\\\";
      }
      std::cout << std::endl;
    }
    std::cout << "\\end{pmatrix}" << std::endl;
  }

  if(aWithNorm) {
    std::cout << "||" << aMatrixName << "||_1     = " << A.norm_1() << std::endl;
    std::cout << "||" << aMatrixName << "||_infty = " << A.norm_infty() << std::endl;
    std::cout << "||" << aMatrixName << "||_F     = " << A.norm_frobenius() << std::endl;
    // std::cout << "||" << aMatrixName << "||       = " << A.norm_sqrt_dot() << std::endl;
  }
}

struct arg_t {
  uint _noBf;
  uint _formats;

  arg_t() : _noBf(2), _formats(0x1) {}
};


bool
parse_args(arg_t& aArgs, const int argc, const char* argv[]) {
  int z = 0;
  char* lCharPtr = 0;
  bool lOk = true;
  int lCurr = 1;

  while(lCurr < argc) {
    if(0 == strcmp("-f", argv[lCurr])) {
      // std::cout << "read format -f" << std::endl;
      ++lCurr;
      if(argc <= lCurr) {
        std::cerr << "-f needs number" << std::endl;
        lOk = false;
        break;
      }
      z = strtol(argv[lCurr], &lCharPtr, 10);
      if(!((lCharPtr != argv[1]) && (1 <= z) && (z < 8))) {
        lOk = false;
        break;
      }
      ++lCurr;
      // std::cout << "f = " << z << std::endl;
      aArgs._formats = z;
    } else {
      z = strtol(argv[lCurr], &lCharPtr, 10);
      // std::cout << "z = " << z << std::endl;
      ++lCurr;
      if(!((lCharPtr != argv[1]) && (1 <= z) && (z <= 14))) {
        lOk = false;
        break;
      }
      aArgs._noBf = z;
    }
  }
  return lOk; 
}

void
fMult(const Matrix& A, const Matrix& B, const char* aMatrixName, const int aWidth) {
  Matrix AB;
  AB.setToProductOf(A,B);
  fPrintAllFormats(AB, 1, aMatrixName, aWidth);
}

void
fElimRows(const Matrix& A, 
          const Matrix& Ainv, 
          const char*   aMatrixName,
          const int     aWidth) {
  const uint lLim = (1 << A.noRows()) - 1;
  uint_vt lRowsToEliminate;

  std::cout << "fElimRows: Input matrix = " << std::endl;
  fPrintAllFormats(A, 1, "A", aWidth);
  std::cout << std::endl;

  Matrix B;
  Matrix Binv;
  Matrix BBinv;
  Matrix BinvB;
  for(uint i = 1; i < lLim; ++i) {
    Bitvector32 lBv(i);
    lRowsToEliminate.clear();
    for(Bitvector32::IteratorElement lIter = lBv.begin(); lIter.isValid(); ++lIter) {
      if(0 != (*lIter)) {
        lRowsToEliminate.push_back(*lIter);
      }
    }
    std::cout << "lRowsToEliminate = " << lRowsToEliminate << std::endl;

    B.setToElimRows(A, lRowsToEliminate);
    fPrintAllFormats(B, 1, "Asub", aWidth);

    Binv.setToElimCols(Ainv, lRowsToEliminate);
    fPrintAllFormats(Binv, 1, "Ainvsub", aWidth);

    BBinv.setToProductOf(B, Binv);
    fPrintAllFormats(BBinv, 1, "BBinv", aWidth);

    BinvB.setToProductOf(Binv, B);
    fPrintAllFormats(BinvB, 1, "BinvB", aWidth);

    assert(BBinv.isEinheitsmatrix(1e-14));
    assert(BBinv.isSymmetric(1e-14));
    // assert(BinvB.isSymmetric(1e-14)); // fails!!!
  }
}

bool
hasColSum1(const Matrix& A) {
  double lSum = 0;
  for(uint j = 0; j < A.noCols(); ++j) {
    lSum = 0;
    for(uint i = 0; i < A.noRows(); ++i) {
      lSum += A(i,j);
    }
    if(1 != lSum) {
      return false;
    }
  }
  return true;
}


// MPI properties
// 1. A   A^+ A   = A  (is only property of g-inverse)
// 2. A^+ A   A^+ = A^+
// 3. (A^+ A)' = A^+ A (i.e., A^+A is symmetric)
// 4. (A A^+)' = A A^+ (i.e., AA^+ is symmetric)
// additionally, check
// 5. A A^+ = I
// 6. A^+ A = I for top m rows/cols

uint32_t
fCheckMpiProps(const Matrix& B, const Matrix& Binv, const bool aTrace) {
  const double lEps = 1E-16;
  const uint   m = B.noRows();
  // const uint   n = B.noCols();
  Matrix BBinv;
  BBinv.setToProductOf(B, Binv);
  Matrix BinvB;
  BinvB.setToProductOf(Binv, B);

  Matrix BinvBBinv;
  BinvBBinv.setToProductOf(BinvB, Binv);
  Matrix BBinvB;
  BBinvB.setToProductOf(BBinv, B);

  const uint32_t lRes0 = B.equalUpTo(BBinvB, lEps);
  const uint32_t lRes1 = Binv.equalUpTo(BinvBBinv, lEps);
  const uint32_t lRes2 = BinvB.isSymmetric();
  const uint32_t lRes3 = BBinv.isSymmetric();

  const uint32_t lRes4 = 0; // keep always 0
  const uint32_t lRes5 = BBinv.isIdentity();
  const uint32_t lRes6 = BinvB.isDiag01UpperTriangular(m);
  const uint32_t lRes7 = hasColSum1(BinvB);

  if(aTrace) {
    std::cout << "fCheckMpiProps:" << std::endl;
    std::cout << "BBinv = " << std::endl; BBinv.print(std::cout, 3) << std::endl;
    std::cout << "BinvB = " << std::endl; BinvB.print(std::cout, 3) << std::endl;
    std::cout << "^^^^^^^^^^^^^^^" << std::endl;
  }

  const uint32_t lRes = (  lRes0 | (lRes1 << 1) | (lRes2 << 2) | (lRes3 << 3) |
                          (lRes4 << 4) | 
                          (lRes5 << 5) | (lRes6 << 6) | (lRes7 << 7)
                        );
  return lRes; 
}

void
fCholesky(const Matrix& M, const uint aFormat, const char* aMatrixName, const int aWidth) {
  std::cout << "Cholesky decomposition:" << std::endl;
  Matrix Minv;
  try {
    Minv.setToSquareRootLowerTriangular(M);
    // Minv.setToSquareRootUpperTriangular(M);
  }
  catch(const Matrix::CholeskyFailed& lException) {
    std::cout << "  Matrix::CholeskyFailed: "
              << lException.index() << ": " << lException.sum()
              << std::endl;
    if(-1E-10 > lException.sum()) {
      return;
    }
  }
  Minv.setZeroSmallerThan(1E-10);
  fPrintAllFormats(Minv, aFormat, (std::string(aMatrixName) + "inv").c_str(), aWidth);

  // Matrix MinvMinvt;
  // MinvMinvt.setToProductWithSecondTransposed(Minv, Minv);
  // std::cout << "prod UUt:" << std::endl;
  // MinvMinvt.print(std::cout, 8);

  std::cout << "end cholesky" << std::endl;
}

void
fSVD(const Matrix& M, const uint aFormat, const char* aMatrixName, const int aWidth) {
  std::cout << "SVD:" << std::endl;
  Matrix U,W,V;
  GMSVD lSvd(M, 0.0000001);

  lSvd.get_U(U);
  std::cout << "U = " << std::endl;
  U.print(std::cout, 1);

  lSvd.get_W(W);
  std::cout << "W = " << std::endl;
  W.print(std::cout, 1);

  lSvd.get_V(V);
  std::cout << "V = " << std::endl;
  V.print(std::cout, 1);


  std::cout << "end SVD" << std::endl;
}

void
fLDL(const Matrix& M, const uint aFormat, const char* aMatrixName, const int aWidth,
     const Matrix* A = 0) {
  std::cout << "LDL decomposition of " << aMatrixName << std::endl;
  std::cout << aMatrixName << " = " << std::endl;
  M.print(std::cout, aWidth);

  std::cout << "(matrix should be symmetric): " 
            << (M.isSymmetric(1e-16) ? "yes" : "no")
            << std::endl;
  Matrix L,D;
  M.getLDL(L,D);
  std::cout << "L = " << std::endl;
  L.print(std::cout, 8);
  std::cout << "D = " << std::endl;
  D.print(std::cout, 8);
  std::cout << "end LDL" << std::endl;

  Matrix LD,LDLt;
  LD.setToProductOf(L,D);
  LDLt.setToProductWithSecondTransposed(LD, L);
  std::cout << "LDL^t = " << std::endl;
  LDLt.print(std::cout, 1);

  std::cout << aMatrixName << " = " << "LDL^t : " << M.equalUpTo(LDLt, 1E-16) << std::endl;
  std::cout << aMatrixName << " is       transposed of L: " << M.isTransposedOf(L) << std::endl;
  std::cout << aMatrixName << " is minor transposed of L: " << M.isMinorTransposedOf(L) << std::endl;
  if(0 != A) {
    std::cout << "L is       transposed of A: " << L.isTransposedOf(*A) << std::endl;
    std::cout << "L is minor transposed of A: " << L.isMinorTransposedOf(*A) << std::endl;
  }
  std::cout << std::endl;
}

void
testLDL() {
  Matrix M(3, 3, 
             4.0,  12.0, -16.0,
            12.0,  37.0, -43.0,
           -16.0, -43.0,  98.0);
  fLDL(M, 1, "M", 5);

  Matrix A(3, 3,
           1.0, 1.0, 1.0,
           1.0, 2.0, 3.0,
           1.0, 3.0, 4.0);
  A.print(std::cout, 1);
  fLDL(A, 1, "A", 5);
}

void
test_1(const uint aNoBf) {
  std::cout << "===============" << std::endl;
  std::cout << "=== TEST -1 ===" << std::endl;
  std::cout << "===============" << std::endl;
  const uint z = aNoBf;
  const uint n = (1 << z);

  Matrix A;
  fGetFullDesignMatrix(A, z);

  uint_vt lIdxVec;
  for(uint i = 1; i < n; i += 2) {
    lIdxVec.push_back(i);
  }

  Matrix E;
  fGetElimMatrix(E, lIdxVec, n);

  Matrix EA;
  EA.setToProductOf(E,A);
  Matrix AE;
  AE.setToProductOf(A,E);

  Matrix Emn;
  const uint m = lIdxVec.size();
  fGetElimMatrix(Emn, lIdxVec, m, n);

  Matrix EmnA;
  EmnA.setToProductOf(Emn,A);
  Matrix AEmnt;
  AEmnt.setToProductWithSecondTransposed(A, Emn);

  Matrix EEt;
  EEt.setToProductWithSecondTransposed(Emn,Emn);

  std::cout << "    A = " << std::endl; A.print(std::cout, 3) << std::endl;
  std::cout << "    E = " << std::endl; E.print(std::cout, 3) << std::endl;
  std::cout << "   AE = " << std::endl; AE.print(std::cout, 3) << std::endl;
  std::cout << "   EA = " << std::endl; EA.print(std::cout, 3) << std::endl;
  std::cout << "  Emn = " << std::endl; Emn.print(std::cout, 3) << std::endl;
  std::cout << " EmnA = " << std::endl; EmnA.print(std::cout, 3) << std::endl;
  std::cout << "AEmnt = " << std::endl; AEmnt.print(std::cout, 3) << std::endl;
  std::cout << "  EEt = " << std::endl; EEt.print(std::cout, 3) << std::endl;
  

}

void
test_2() {
  std::cout << "===============" << std::endl;
  std::cout << "=== TEST -2 ===" << std::endl;
  std::cout << "===============" << std::endl;
  const uint lNoBf = 2;
  const uint z = lNoBf;
  const uint n = (1 << z);

  Matrix A;
  fGetFullDesignMatrix(A, z);

  uint_vt lIdxVec;
  for(uint i = 0; i < (n-1); ++i) {
    lIdxVec.push_back(i);
  }

  Matrix E;
  const uint m = lIdxVec.size();
  fGetElimMatrix(E, lIdxVec, m, n);

  Matrix B;
  B.setToProductOf(E,A);

  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);

  Matrix Binv;
  Binv.setToProductWithSecondTransposed(Ainv, E);

  Matrix BinvB;
  BinvB.setToProductOf(Binv,B);

  Matrix BBinv;
  BBinv.setToProductOf(B, Binv);

  std::cout << "A = " << std::endl; A.print(std::cout, 3) << std::endl;
  std::cout << "E = " << std::endl; E.print(std::cout, 3) << std::endl;
  std::cout << "B = " << std::endl; B.print(std::cout, 3) << std::endl;
  std::cout << "Ainv = " << std::endl; Ainv.print(std::cout, 3) << std::endl;
  std::cout << "Binv = " << std::endl; Binv.print(std::cout, 3) << std::endl;
  std::cout << "BinvB = " << std::endl; BinvB.print(std::cout, 3) << std::endl;
  std::cout << "BBinv = " << std::endl; BBinv.print(std::cout, 3) << std::endl;
  
}

void
test_3() {
  std::cout << "===============" << std::endl;
  std::cout << "=== TEST -3 ===" << std::endl;
  std::cout << "===============" << std::endl;
  const uint lNoBf = 2;
  const uint z = lNoBf;
  const uint n = (1 << z);

  Matrix A;
  fGetFullDesignMatrix(A, z);

  uint_vt lIdxVec;
  for(uint i = 0; i < n; ++i) {
    if(1 != i) {
      lIdxVec.push_back(i);
    }
  }

  Matrix E;
  const uint m = lIdxVec.size();
  fGetElimMatrix(E, lIdxVec, m, n);

  Matrix B;
  B.setToProductOf(E,A);

  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);

  Matrix Binv;
  Binv.setToProductWithSecondTransposed(Ainv, E);

  Matrix BinvB;
  BinvB.setToProductOf(Binv,B);

  Matrix BBinv;
  BBinv.setToProductOf(B, Binv);

  Matrix I;
  I.setToDiag(1.0, n);

  Matrix I_BinvB;
  I_BinvB.setToDifferenceOf(I, BinvB);

  std::cout << "T = " << lIdxVec << std::endl;
  std::cout << "A = " << std::endl; A.print(std::cout, 3) << std::endl;
  std::cout << "E = " << std::endl; E.print(std::cout, 3) << std::endl;
  std::cout << "B = " << std::endl; B.print(std::cout, 3) << std::endl;
  std::cout << "I = " << std::endl; I.print(std::cout, 3) << std::endl;
  std::cout << "Ainv = " << std::endl; Ainv.print(std::cout, 3) << std::endl;
  std::cout << "Binv = " << std::endl; Binv.print(std::cout, 3) << std::endl;
  std::cout << "BinvB = " << std::endl; BinvB.print(std::cout, 3) << std::endl;
  std::cout << "BBinv = " << std::endl; BBinv.print(std::cout, 3) << std::endl;
  std::cout << "I - BinvB = " << std::endl; I_BinvB.print(std::cout, 3) << std::endl;

  double b[4] = {0, 0, 0, 0};
  double x[4] = {0, 0, 0, 0};
  double Ax[4] = {0, 0, 0, 0};

  b[0] = 100;
  b[1] =  50;
  b[2] =  51;

  Binv.getAx(x, b);
  A.getAx(Ax, x);

  std::cout << " b = "; Matrix::printVector(std::cout, b, 3, 4) << std::endl;
  std::cout << " x = "; Matrix::printVector(std::cout, x, 4, 4) << std::endl;
  std::cout << "Ax = "; Matrix::printVector(std::cout, Ax, 4, 4) << std::endl;
  std::cout << std::endl;

  b[0] = 100;
  b[1] =  50;
  b[2] =  25;

  Binv.getAx(x, b);
  A.getAx(Ax, x);

  std::cout << " b = "; Matrix::printVector(std::cout, b, 3, 4) << std::endl;
  std::cout << " x = "; Matrix::printVector(std::cout, x, 4, 4) << std::endl;
  std::cout << "Ax = "; Matrix::printVector(std::cout, Ax, 4, 4) << std::endl;
  std::cout << std::endl;

  double a = 0;

  double w[4] = {0, 0, 0, 0};
  double y[4] = {0, 0, 0, 0};
  a = 25;
  y[0] = -a; y[1] = +a;
  Matrix::vecSum(w, x, y, 4);
  A.getAx(Ax, w);
  std::cout << " x = "; Matrix::printVector(std::cout, x, 4, 4) << std::endl;
  std::cout << " y = "; Matrix::printVector(std::cout, y, 4, 4) << std::endl;
  std::cout << " w = "; Matrix::printVector(std::cout, w, 4, 4) << std::endl;
  std::cout << "Aw = "; Matrix::printVector(std::cout, Ax, 4, 4) << std::endl;
  std::cout << std::endl;

  a = 75;
  y[0] = -a; y[1] = +a;
  Matrix::vecSum(w, x, y, 4);
  A.getAx(Ax, w);
  std::cout << " x = "; Matrix::printVector(std::cout, x, 4, 4) << std::endl;
  std::cout << " y = "; Matrix::printVector(std::cout, y, 4, 4) << std::endl;
  std::cout << " w = "; Matrix::printVector(std::cout, w, 4, 4) << std::endl;
  std::cout << "Aw = "; Matrix::printVector(std::cout, Ax, 4, 4) << std::endl;
  std::cout << std::endl;

  a = 50;
  y[0] = -a; y[1] = +a;
  Matrix::vecSum(w, x, y, 4);
  A.getAx(Ax, w);
  std::cout << " x = "; Matrix::printVector(std::cout, x, 4, 4) << std::endl;
  std::cout << " y = "; Matrix::printVector(std::cout, y, 4, 4) << std::endl;
  std::cout << " w = "; Matrix::printVector(std::cout, w, 4, 4) << std::endl;
  std::cout << "Aw = "; Matrix::printVector(std::cout, Ax, 4, 4) << std::endl;
  std::cout << std::endl;

}

void
test_4() {
  std::cout << "===============" << std::endl;
  std::cout << "=== TEST -4 ===" << std::endl;
  std::cout << "===============" << std::endl;

  const uint lNoBf = 2;
  const uint z = lNoBf;
  const uint n = (1 << z);

  Matrix C;
  fGetFullDesignMatrix(C, z);

  Matrix Cinv;
  Cinv.setToInverseOfUpperTriangular(C, 1E-16);

  uint_vt lRowsToRetain;
  for(uint i = 0; i < n; ++i) {
    lRowsToRetain.clear();
    for(BvMemberIdxAsc<uint64_t> lIter((uint64_t) 1 | (i << 1)); lIter.isValid(); ++lIter) {
      lRowsToRetain.push_back(*lIter);
    }
    Matrix D;
    D.setToOnlyRows(C, lRowsToRetain);
    Matrix Dinv;
    Dinv.setToOnlyCols(Cinv, lRowsToRetain);
    Matrix DinvD;
    DinvD.setToProductOf(Dinv, D);
    std::cout << "i = "; Bitvector32(i).print(std::cout, 2) << std::endl;
    std::cout << "D^-D = " << std::endl;
    DinvD.printLatex(std::cout, 3, 3) << std::endl << std::endl;

    Matrix DinvD_inv;
    DinvD_inv.setToGInverseOfUpperTriangular(DinvD, 1E-16);
    std::cout << "(D^-D)^{-} = " << std::endl;
    DinvD_inv.printLatex(std::cout, 3, 3) << std::endl << std::endl;

    const uint32_t lMpiProps = fCheckMpiProps(DinvD, DinvD_inv, true);
    std::cout << "mpi props of D^-D)^{-} :";
    Bitvector32(lMpiProps).print(std::cout, 10) << std::endl << std::endl;

  }
}

void
test_5(const uint aNoBf) {
  std::cout << "===============" << std::endl;
  std::cout << "=== TEST -5 ===" << std::endl;
  std::cout << "===============" << std::endl;

  const uint lNoBf = aNoBf;
  const uint z = lNoBf;
  const uint n = (1 << z);
  const uint s = (1 << n);

  Matrix C;
  fGetFullDesignMatrix(C, z);

  Matrix CaddCt;
  CaddCt.setToSumWithSecondTransposed(C,C);

  fCholesky(CaddCt, 1, "CaddCt", 1);
  fSVD(CaddCt, 1, "CaddCt", 1);

  Matrix Cinv;
  Cinv.setToInverseOfUpperTriangular(C, 1E-16);

  uint_vt lRowsToRetain;
  for(uint i = 1; i < s; ++i) {
    lRowsToRetain.clear();
    for(BvMemberIdxAsc<uint64_t> lIter(i); lIter.isValid(); ++lIter) {
      lRowsToRetain.push_back(*lIter);
    }
    std::cout << "i = "; Bitvector32(i).print(std::cout, n) << std::endl;
    Matrix D;
    D.setToOnlyRows(C, lRowsToRetain);
    Matrix Dinv;
    Dinv.setToOnlyCols(Cinv, lRowsToRetain);
    Matrix DinvD;
    DinvD.setToProductOf(Dinv, D);
    std::cout << "D^-D = " << std::endl;
    DinvD.print(std::cout, 3) << std::endl << std::endl;

    Matrix I;
    I.setToDiag(1.0, n);
    Matrix F;
    F.setToDifferenceOf(I, DinvD);
    
    std::cout << "F := (I - D^-D) = " << std::endl;
    F.print(std::cout, 3) << std::endl << std::endl;

    uint_vt lRows;
    uint_vt lCols;


    F.collectNonZeroRowCol(lRows, lCols, 0, 1E-16);
    const uint lSizeFred = lRows.size() * lCols.size();
    if(0 < lRows.size()) {
      Matrix Fred;
      Fred.setToReduction(F, lRows, lCols);
      std::cout << "red(F) = " << std::endl;
      Fred.print(std::cout, 3) << std::endl << std::endl;
   
      Matrix Fredinv;
      Fredinv.setToMoorePenroseInverseOf(Fred, 1E-10, false);
      std::cout << "red(F)^+ = " << std::endl;
      Fredinv.print(std::cout, 3) << std::endl << std::endl;
    }

    Matrix FFt;
    FFt.setToProductWithSecondTransposed(F, F);
    
    std::cout << "FF^t = " << std::endl;
    FFt.print(std::cout, 3) << std::endl << std::endl;

    FFt.collectNonZeroRowCol(lRows, lCols, 0, 1E-16);
    const uint lSizeFFtred = lRows.size() * lCols.size();

    if(lSizeFred != lSizeFFtred) {
      std::cout << "@ " << lSizeFred << ' ' << lSizeFFtred << ' '
                << ((lSizeFred < lSizeFFtred) ? "F XXX" : "FFt YYY")
                << std::endl;
    }

    if(0 < lRows.size()) { 
      Matrix FFtred;
      FFtred.setToReduction(FFt, lRows, lCols);
      std::cout << "red(FF^t) = " << std::endl;
      FFtred.print(std::cout, 3) << std::endl << std::endl;
      Matrix U;
      U.setToMoorePenroseInverseOf(FFtred, 1E-10, false);
      std::cout << "U = " << std::endl;
      U.print(std::cout, 5) << std::endl << std::endl;
/*
      try {
        U.setToSquareRootUpperTriangular(FFtred);
        std::cout << "U = " << std::endl;
        U.print(std::cout, 5) << std::endl << std::endl;
        Matrix UUt;
        UUt.setToProductWithSecondTransposed(U, U);
        std::cout << "UUt = " << std::endl;
        UUt.print(std::cout, 5) << std::endl << std::endl;
      }
      catch(const Matrix::CholeskyFailed& lExc) {
        std::cout << "Cholesky failed: " << lExc.sum() << std::endl;
      }
*/

    }
  }
}

inline
bool
cinv_is_negative(const uint i, const uint j) {
  return (number_of_bits_set<uint>(i ^ j) & 0x1);
}

void
fCinvx_a(double_vt& x, const double_vt& b, const uint aNoBf) {
  const uint n = (1 << aNoBf);
  assert(b.size() == n);
  assert(x.size() == n);
  for(uint i = 0; i < n; ++i) {
    x[i] = 0;
  }
  const uint lMask = (1 << aNoBf) - 1;
  double lSum = 0; // sum of one row of Cinv
  uint   lCi = 0;  // complement of row index i
  uint   j;        // column index j
  for(uint i = 0; i < n; ++i) {
    lSum = 0;
    lCi = (~i & lMask);
    for(BvAllSubsets<uint> lIter(lCi); lIter.isValid(); ++lIter) {
      j = (i | (*lIter));
      if(cinv_is_negative(i,j)) {
        lSum -= b[j];
      } else {
        lSum += b[j];
      }
    }
    x[i] = lSum;
  }
}

void
fCinvx_b(double_vt& x, const double_vt& b, const uint aNoBf) {
  const uint n = (1 << aNoBf);
  assert(b.size() == n);
  assert(x.size() == n);
  for(uint i = 0; i < n; ++i) {
    x[i] = 0;
  }
  const uint lMask = (1 << aNoBf) - 1;
  double lSum = 0; // sum of one row of Cinv
  uint   lCi = 0;  // complement of row index i
  uint   lCj = 0;  // content of *lIter, 
  uint   j = 0;    // column index j = lCi | lCj
  uint   k = 0;    // number of bits set of lCj
  for(uint i = 0; i < n; ++i) {
    lSum = 0;
    lCi = (~i & lMask);
    for(BvAllSubsets<uint> lIter(lCi); lIter.isValid(); ++lIter) {
      lCj = (*lIter);
      k = number_of_bits_set<uint>(lCj) & 0x1;
      j = (i | lCj);
      if(k) {
        lSum -= b[j];
      } else {
        lSum += b[j];
      }
    }

    x[i] = lSum;
  }
}

void
convertBeta2Gamma(double_vt& x, const double_vt& b, const uint aNoBf) {
  const uint n = (1 << aNoBf);
  assert(b.size() == n);
  assert(x.size() == n);
  for(uint i = 0; i < n; ++i) {
    x[i] = 0;
  }
  const double lFactor[2] = {1.0, -1.0};
  const uint lMask = (1 << aNoBf) - 1;
  double lSum = 0; // sum of one row of Cinv
  uint   lCi = 0;  // complement of row index i
  uint   lCj = 0;  // content of *lIter, 
  uint   j = 0;    // column index j = lCi | lCj
  uint   k = 0;    // number of bits set of lCj
  for(uint i = 0; i < n; ++i) {
    lSum = 0;
    lCi = (~i & lMask);
    for(BvAllSubsets<uint> lIter(lCi); lIter.isValid(); ++lIter) {
      lCj = (*lIter);
      k = number_of_bits_set<uint>(lCj) & 0x1;
      j = (i | lCj);
      lSum += lFactor[k] * b[j];
    }

    x[i] = lSum;
  }
}

void convertBeta2GammaRecPos(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf);
void convertBeta2GammaRecNeg(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf);

void
convertBeta2GammaRec(double_vt& x, const double_vt& b, const uint aNoBf) {
  if(1 == aNoBf) {
    x[0] = b[0] - b[1];
    x[1] = b[1];
    return;
  }

  const uint n = (1 << aNoBf);
  for(uint i = 0; i < n; ++i) {
    x[i] = 0;
  }

  convertBeta2GammaRecPos(x, b, 0, 0, aNoBf);
}

void
convertBeta2GammaRecPos(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf) {
  const uint i = aIdxX;
  const uint j = aIdxB;
  // std::cout << "fCinvx_rec_pos(" << i << ',' << j << ',' << aNoBf << ')' << std::endl;

  if(2 == aNoBf) {
    x[i+0] +=   b[j+0] - b[j+1] - b[j+2] + b[j+3];
    x[i+1] +=            b[j+1]          - b[j+3];
    x[i+2] +=                     b[j+2] - b[j+3];
    x[i+3] +=                              b[j+3];
    return;
  }
  const uint h = (1 << (aNoBf - 1)); // half the array in both dimensions
  convertBeta2GammaRecPos(x, b, i+0, j+0, aNoBf - 1);
  convertBeta2GammaRecNeg(x, b, i+0, j+h, aNoBf - 1);
  convertBeta2GammaRecPos(x, b, i+h, j+h, aNoBf - 1);
}

void
convertBeta2GammaRecNeg(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf) {
  const uint i = aIdxX;
  const uint j = aIdxB;
  // std::cout << "fCinvx_rec_neg(" << i << ',' << j << ',' << aNoBf << ')' << std::endl;

  if(2 == aNoBf) {
    x[i+0] +=  -b[j+0] + b[j+1] + b[j+2] - b[j+3];
    x[i+1] +=          - b[j+1]          + b[j+3];
    x[i+2] +=                   - b[j+2] + b[j+3];
    x[i+3] +=                            - b[j+3];
    return;
  }
  const uint h = (1 << (aNoBf - 1)); // half the array in both dimensions
  convertBeta2GammaRecNeg(x, b, i+0, j+0, aNoBf - 1);
  convertBeta2GammaRecPos(x, b, i+0, j+h, aNoBf - 1);
  convertBeta2GammaRecNeg(x, b, i+h, j+h, aNoBf - 1);
}

void
fPrintFullDesignMatrix(const arg_t& aArg) {
  const uint z = (uint) aArg._noBf;
  const uint n = (1 << z);

  Matrix C;
  fGetFullDesignMatrix(C, z);
  if(6 > z) {
    fPrintAllFormats(C, aArg._formats, "C", 2);
  }

  Matrix CtC;
  CtC.setToProductWithFirstTransposed(C, C);
  if(6 > z) {
    fPrintAllFormats(CtC, aArg._formats, "C^tC", 2);
  }

  Matrix Cinv;

  Measure lMeasureCinv;
  lMeasureCinv.start();
  Cinv.setToInverseOfUpperTriangular(C, 1E-10);
  lMeasureCinv.stop();
  std::cout << "time to invert C: " << lMeasureCinv.mTotalTime() << " [s]" << std::endl;
  if(6 > z) {
    fPrintAllFormats(Cinv, aArg._formats, "C^{-1}", 2);
  }


  if(6 > z) {
    Bitvector32 lBv;
    uint lIsNegative = 0;
    std::cout << "List of C[i,j] != 0:" << std::endl;
    for(uint i = 0; i < n; ++i) {
      for(uint j = i; j < n; ++j) {
        if(0 == Cinv(i,j)) {
          continue;
        }
        std::cout << std::setw(3) << i << ' '
                  << std::setw(3) << j << ' '
                  << std::setw(3) << Cinv(i,j) << ' ';
        lBv = i;
        lBv.print(std::cout, z) << ' ';
        lBv = j;
        lBv.print(std::cout, z) << ' ';
        lBv = (i ^ j);
        lBv.print(std::cout, z) << ' ';
        lIsNegative = (lBv.cardinality() & 0x1);
        std::cout << std::setw(2) << lIsNegative << ' ';
        if(((Cinv(i,j) < 0) && (1 == lIsNegative)) ||
           ((Cinv(i,j) > 0) && (0 == lIsNegative))) {
          std::cout << "ok";
        } else {
          std::cout << "bad";
          assert(0 == 1);
        }
        std::cout << std::endl;
      }
    }
  }

  double_vt b(n);
  double_vt x1(n);
  double_vt x2(n);
  double_vt x3(n);
  double_vt x4(n);
  double_vt x5(n);
  for(uint i = 0; i < n; ++i) {
    b[i] = sin(i * (i + 1));
  }
  Measure lMeasure1;
  lMeasure1.start();
  Cinv.getAx(x1.data(), b.data());
  lMeasure1.stop();

  Measure lMeasure2;
  lMeasure2.start();
  fCinvx_a(x2, b, z);
  lMeasure2.stop();
  
  Measure lMeasure3;
  lMeasure3.start();
  fCinvx_b(x3, b, z);
  lMeasure3.stop();
  
  Measure lMeasure4;
  lMeasure4.start();
  convertBeta2Gamma(x4, b, z);
  lMeasure4.stop();
 
  Measure lMeasure5;
  lMeasure5.start();
  convertBeta2GammaRec(x5, b, z);
  lMeasure5.stop(); 

  if(6 > z) {
    std::cout << "x1 = " << x1 << std::endl;
    std::cout << "x2 = " << x2 << std::endl;
    std::cout << "x3 = " << x3 << std::endl;
    std::cout << "x4 = " << x4 << std::endl;
    std::cout << "x5 = " << x5 << std::endl;
  }

  if(is_equal(x1, x2, 1E-10) && 
     is_equal(x1, x3, 1E-10) &&
     is_equal(x1, x4, 1E-10) &&
     is_equal(x1, x5, 1E-10)) {
    std::cout << "z = " << z << std::endl;
    std::cout << "n = " << n << std::endl;
    std::cout << "x1 == x2 == x3 == x4 == x5" << std::endl;
    std::cout << "runtime matrix  mult: " << (lMeasure1.mTotalTime() * 1000.0) << " [ms]" << std::endl;
    std::cout << "        special a   : " << (lMeasure2.mTotalTime() * 1000.0) << " [ms]" << std::endl;
    std::cout << "        special b   : " << (lMeasure3.mTotalTime() * 1000.0) << " [ms]" << std::endl;
    std::cout << "        special c   : " << (lMeasure4.mTotalTime() * 1000.0) << " [ms]" << std::endl;
    std::cout << "        special rec : " << (lMeasure5.mTotalTime() * 1000.0) << " [ms]" << std::endl;
    std::cout << "              factor: " << (lMeasure1.mTotalTime() / lMeasure5.mTotalTime()) << std::endl;
  } else {
    std::cout << "x1 != x2  BAD" << std::endl;
  }
  assert(is_equal(x1, x2, 1E-16));
}


void
convertGamma2Beta(double_vt& aBeta, const double_vt& aGamma, const uint aNoBf) {
  const uint n = (1 << aNoBf);
  assert(aGamma.size() == n);
  aBeta.resize(n);
  for(uint i = 0; i < n; ++i) {
    aBeta[i] = 0;
  }

  // std::cout << "convertGamma2Beta:" << std::endl;

  const uint lMask = (1 << aNoBf) - 1;
  for(uint i = 0; i < n; ++i) {
    double lSum = 0;
    uint lCi  = (~i & lMask);
    // std::cout << i << std::endl;
    for(BvAllSubsets<uint> lIter(lCi); lIter.isValid(); ++lIter) {
      const uint lIdx = (i | (*lIter));
      lSum += aGamma[lIdx];
      // std::cout << "  " << std::setw(3) << lIdx 
      //           << "  " << std::setw(5) << aGamma[lIdx] 
      //           << "  " << std::setw(5) << lSum
      //           << std::endl;
    }
    aBeta[i] = lSum;
    // std::cout << "aBeta[" << i << "] = " << lSum << std::endl;
  }
}

void convertGamma2BetaRecSub(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf);

void
convertGamma2BetaRec(double_vt& x, const double_vt& b, const uint aNoBf) {
  if(1 == aNoBf) {
    x[0] = b[0] + b[1];
    x[1] = b[1];
    return;
  }

  const uint n = (1 << aNoBf);
  for(uint i = 0; i < n; ++i) {
    x[i] = 0;
  }

  convertGamma2BetaRecSub(x, b, 0, 0, aNoBf);
}

void
convertGamma2BetaRecSub(double_vt& x, const double_vt& b, const uint aIdxX, const uint aIdxB, const uint aNoBf) {
  const uint i = aIdxX;
  const uint j = aIdxB;
  // std::cout << "fCinvx_rec_pos(" << i << ',' << j << ',' << aNoBf << ')' << std::endl;

  if(2 == aNoBf) {
    x[i+0] +=   b[j+0] + b[j+1] + b[j+2] + b[j+3];
    x[i+1] +=            b[j+1]          + b[j+3];
    x[i+2] +=                     b[j+2] + b[j+3];
    x[i+3] +=                              b[j+3];
    return;
  }
  const uint h = (1 << (aNoBf - 1)); // half the array in both dimensions
  convertGamma2BetaRecSub(x, b, i+0, j+0, aNoBf - 1);
  convertGamma2BetaRecSub(x, b, i+0, j+h, aNoBf - 1);
  convertGamma2BetaRecSub(x, b, i+h, j+h, aNoBf - 1);
}



void
fTestGamma2Beta(const arg_t& aArg) {
  std::cout << "test gamma2beta:" << std::endl;
  std::cout << "m1: standard procedure convertGamma2Beta" << std::endl;
  std::cout << "m2: recursive convertGamma2BetaRec" << std::endl;
  const uint z = (uint) aArg._noBf;
  const uint n = (1 << z);

  Matrix C;
  fGetFullDesignMatrix(C, z);
  if(6 > z) {
    fPrintAllFormats(C, aArg._formats, "C", 3);
  }


  double_vt x(n);
  double_vt b1(n);
  double_vt b2(n);

  for(uint i = 0; i < n; ++i) {
    x[i] = sin(i * (i + 1));
  }

  Measure lMeasure1;
  lMeasure1.start();
  convertGamma2Beta(b1, x, z);
  lMeasure1.stop();

  Measure lMeasure2;
  lMeasure2.start();
  convertGamma2BetaRec(b2, x, z);
  lMeasure2.stop();


  if(is_equal(b1, b2, 1E-10)) {
    std::cout << "b1 == b2" << std::endl;
    std::cout << "  m1 = " << (lMeasure1.mTotalTime() * 1000) << " [ms]" << std::endl;
    std::cout << "  m2 = " << (lMeasure2.mTotalTime() * 1000) << " [ms]" << std::endl;
    std::cout << "m1/2 = " << (lMeasure1.mTotalTime() / lMeasure2.mTotalTime()) << std::endl;
    if(6 > z) {
      std::cout << "b1 = " << b1 << std::endl;
      std::cout << "b2 = " << b2 << std::endl;
    }
  } else {
    std::cout << "b1 != b2" << std::endl;
    std::cout << "b1 = " << b1 << std::endl;
    std::cout << "b2 = " << b2 << std::endl;
  }

}




void
test0(const arg_t& aArg) {
  const bool lDoElimRows = true;

  const uint z = (uint) aArg._noBf;
  Matrix A;
  fGetFullDesignMatrix(A, z);
  fPrintAllFormats(A, aArg._formats, "A", 1);
  std::cout << "A is minor symmetric: " << A.isMinorSymmetric() << std::endl;
  std::cout << "A is       symmetric: " << A.isSymmetric() << std::endl;

  // fCholesky(A, 1, "A", 1);
  // fSVD(A, 1, "A", 1);
  // fLDL(A, 1, "A", 1);

  Matrix Ainv;
  Ainv.setToMoorePenroseInverseOf(A);
  std::cout << "Matrix::setToMoorePenroseInverseOf: " << std::endl;
  fPrintAllFormats(Ainv, aArg._formats, "Ainv", 2);

  Matrix Ainv2;
  Ainv2.setToInverseOfUpperTriangular(A, 0.00000001);
  std::cout << "Matrix::setToInverseOfUpperTriangular: " << std::endl;
  fPrintAllFormats(Ainv2, aArg._formats, "Ainv2", 2);

  std::cout << "Ainv == Ainv2: " << (Ainv.equalUpTo(Ainv2, 0) ? "yes" : "no") 
            << ", absdiff = " << Ainv.abserror(Ainv2) 
            << std::endl;

  fMult(A, Ainv, "AAinv", 1);
  fMult(Ainv, A, "AinvA", 1);

  if(lDoElimRows) {
    fElimRows(A, Ainv, "A", 1);
  }

  Matrix AAt, AtA;
  AAt.setToProductWithSecondTransposed(A, A);
  AtA.setToProductWithFirstTransposed(A, A);
  std::cout << "AAt is    symmetric: " << AAt.isSymmetric() << std::endl;
  std::cout << "AAt is persymmetric: " << AAt.isMinorSymmetric() << std::endl;
  std::cout << "AtA is    symmetric: " << AtA.isSymmetric() << std::endl;
  std::cout << "AtA is persymmetric: " << AtA.isMinorSymmetric() << std::endl;
  std::cout << "AtA = (AAt)^T   : " << AtA.isMinorTransposedOf(AAt, 10E-15) << std::endl;
  std::cout << "AAt = (AtA)^T   : " << AAt.isMinorTransposedOf(AtA, 10E-15) << std::endl;
  fPrintAllFormats(AAt, 1, "AAt", 2);
  fPrintAllFormats(AtA, 1, "AtA", 2);

  fLDL(AAt, 1, "AAt", 3, &A);
  fLDL(AtA, 1, "AtA", 3, &A); // yes: A = L^t !!!
}

void
test1(const arg_t& aArg) {
  std::cout << "==============" << std::endl;
  std::cout << "=== TEST 1 ===" << std::endl;
  std::cout << "==============" << std::endl;
  const uint z = (uint) aArg._noBf;
  const uint n = (1 << z);
  double_vt v(n);
  double_vt vsqrt(n);
  double_vt vinv(n);
  double_vt vsqrtinv(n);
  fFillVector(v.data(), n);
  fSqrtVector(vsqrt.data(), v.data(), n);
  fInvVector(vinv.data(), v.data(), n);
  fInvVector(vsqrtinv.data(), vsqrt.data(), n);
  std::cout << "v = " << v << std::endl;
  Matrix A;
  fGetFullDesignMatrix(A, z);

  // A: design matrix
  // B: partial design matrix
  // D: diag(v)
  // E: diag(sqrt(v))
  // goal: calculate (BDB')^{-1}

  Matrix B;
  Matrix Binv;
  Matrix BDBt;
  Matrix BDBtinv;
  Matrix BE;
  Matrix BEinv;
  Matrix BEt;
  Matrix BEtinv;
  Matrix BEinvtBEinv;
  Matrix M, M1, M2; 
  Matrix D,E,F;
  Matrix I;

  D.setToDiag(v.data(), v.size());
  fPrintAllFormats(D, 1, "D", 9);
  E.setToDiag(vsqrt.data(), vsqrt.size());
  fPrintAllFormats(E, 1, "E", 9);
  F.setToDiag(vsqrtinv.data(), vsqrtinv.size());
  fPrintAllFormats(F, 1, "F", 9);

  const uint lLim = (1 << A.noRows()) - 1;
  uint_vt lRowsToEliminate;
  for(uint i = 1; i < lLim; ++i) {
    Bitvector32 lBv(i);
    lRowsToEliminate.clear();
    for(Bitvector32::IteratorElement lIter = lBv.begin(); lIter.isValid(); ++lIter) {
      if(0 != (*lIter)) {
        lRowsToEliminate.push_back(*lIter);
      }
    }
    if(3 > (A.noRows() - lRowsToEliminate.size())) {
      continue;
    }
    std::cout << "lRowsToEliminate = " << lRowsToEliminate << std::endl;
    std::cout << "      v       = " << v << std::endl;
    std::cout << "sqrt(v)       = " << vsqrt << std::endl;
    std::cout << "inv(v)        = " << vinv << std::endl;
    std::cout << "inv(sqrt(v))  = " << vsqrtinv << std::endl;

    // set B to subset of rows of A
    B.setToElimRows(A, lRowsToEliminate);
    fPrintAllFormats(B, 1, "B", 1);

    // calculate B+
    Binv.setToMoorePenroseInverseOf(B);
    fPrintAllFormats(Binv, 1, "B+", 4);

    // check BB+ = I
    M.setToProductOf(B,Binv);
    M.setZeroSmallerThan(1E-15);
    fPrintAllFormats(M, 1, "BB+", 1);
    I.setToDiag(1.0, B.noRows());
    // fPrintAllFormats(I, 1, "I", 1);
    if(!(M.equalUpTo(I, 1e-10))) {
      std::cout << "BB+ != I  (eps = 1e-10)" << std::endl;
    }

    // calculate BDBt
    BDBt.setToAdiagvAt(B, v.data());
    fPrintAllFormats(BDBt, 1, "BDBt", 9);

    // calculate (BDBt)+
    BDBtinv.setToMoorePenroseInverseOf(BDBt);
    fPrintAllFormats(BDBtinv, 1, "(BDBt)+", 9);

    M.setToProductOf(BDBt, BDBtinv);
    M.setZeroSmallerThan(1E-10);
    fPrintAllFormats(M, 1, "(BDBt)(BDBt)+", 9);

    BE.setToProductWithDiagAsVector(B, vsqrt.data());
    // fPrintAllFormats(BE, 1, "BE", 9);


    // check calculations so far
    M.setToProductWithSecondTransposed(BE, BE);
    fPrintAllFormats(M, 1, "(BE)(BE)'", 9);

    
    if(!(M.equalUpTo(BDBt, 1e-16))) {
      std::cout << "(BE)(BE)' != BDBt'  (eps = 1e-16)" << std::endl;
      assert(M.equalUpTo(BDBt, 1e-15));
    }
    // end check


    // bis hierhin alles o.k.:
    // (BE)(BE)' = BDB'

    // (BE)+
    BEinv.setToMoorePenroseInverseOf(BE);
    // BEinv.setZeroSmallerThan(1E-15);
    fPrintAllFormats(BEinv, 1, "(BE)+", 9);

    BEt.setToTransposedOf(BE);
    BEtinv.setToMoorePenroseInverseOf(BEt);
    fPrintAllFormats(BEtinv, 1, "((BE)')+", 9);


    // simply some printing
    M.setToProductOf(BE, BEinv);
    M.setZeroSmallerThan(1E-15);
    fPrintAllFormats(M, 1, "(BE)(BE)+", 9);

    M.setToProductOf(BEinv, BE);
    M.setZeroSmallerThan(1E-15);
    fPrintAllFormats(M, 1, "(BE)+(BE)", 9);

    // testing of setToProductWithDiagAsVector
    M.setToProductWithDiagAsVector(vsqrtinv.data(), Binv);
    fPrintAllFormats(M, 1, "M  = E+B+", 9);

    M1.setToProductOf(F, Binv);
    fPrintAllFormats(M1, 1, "M1 = E+B+", 9);
    assert(M.equalUpTo(M1,0));
    // end testing setToProductWithDiagAsVector
    
    // calculate ((BE)^{-1})^t (BE)^{-1}
    M1.setToTransposedOf(BEinv);
    M.setToProductOf(M1, BEinv);

    BEinvtBEinv.setToProductWithFirstTransposed(BEinv, BEinv);

    std::cout << "Final check: " << std::endl << std::endl;

    fPrintAllFormats(BDBtinv, 1, "(BDBt)+", 9);
    fPrintAllFormats(BEinvtBEinv, 1, "(BE)'+(BE)+", 9);

    assert(M.equalUpTo(BEinvtBEinv, 0));

    M.setToDifferenceOf(BEinvtBEinv, BDBtinv);
    std::cout << "max DIFF = " << M.max() << std::endl;
    fPrintAllFormats(M, 1, "DIFF", 9);

  }

}

void
test2(const uint aNoBf) {
  Matrix A;
  fGetFullDesignMatrix(A, aNoBf);
  std::cout << "full design matrix:" << std::endl;
  A.print(std::cout, 1);
  // A.printMatlab(std::cout, 1);
}

bool 
idx_set_is_subset(const uint i, const uint j) {
  return (i == (i | j));
}

void
test3(const uint aNoBf, const bool aTrace) {
  const uint n = (1 << aNoBf);
  if(aTrace) {
    std::cout << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "ROW CONTAINMENT TEST for aNoBf = " << aNoBf << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << std::endl;
  }

  Matrix A;
  fGetFullDesignMatrix(A, aNoBf);
  if(aTrace) {
    std::cout << "full design matrix:" << std::endl;
    A.print(std::cout, 1);
    std::cout << std::endl;
    std::cout << "row containment: " << std::endl;
    for(uint i = 0; i < n; ++i) {
      std::cout << std::setw(2) << i << ":  ";
      for(uint j = 0; j <= i; ++j) {
        std::cout << idx_set_is_subset(i,j) << ' ';
      }
      std::cout << std::endl;
    }
  }

  Matrix B(n,n);
  for(uint i = 0; i < n; ++i) {
    for(uint j = 0; j < n; ++j) {
      B(j,i) = (double) (int) idx_set_is_subset(i,j);
    }
  }
  if(aTrace) {
    std::cout << "B = " << std::endl;
    B.print(std::cout, 1);
  }
  std::cout << "z = " << std::setw(2) << aNoBf 
            << ":  A == B = " << A.equalUpTo(B, 0) << std::endl; 
}


bool
isInverse(const Matrix& A, const Matrix& Ainv) {
  Matrix AAinv;
  AAinv.setToProductOf(A, Ainv);
  Matrix AinvA;
  AinvA.setToProductOf(Ainv, A);
  return (AAinv.isIdentity() && AinvA.isIdentity());
}

bool
isAbs01(const Matrix& M) {
  const double lEps = 1e-10;
  for(uint i = 0; i < M.noRows(); ++i) {
    for(uint j = 0; j < M.noCols(); ++j) {
      const double x = fabs(M(i,j));
      if((1.0 + lEps) < x) {
        return false;
      }
      if(lEps >= x) {
        continue;
      }
      // eps < x <= 1 + eps
      if((1.0 - lEps) <= x) {
        // 1 - eps <= x <= 1 + eps  o.k.
        continue;
      }
      // eps < x <= 1 + eps
      // and
      // x < 1 - eps
      // thus
      // eps < x < 1 - eps
      return false;
    }
  }
  return true;
}

bool
hasSameZero(const Matrix& A, const Matrix& B) {
  const double lEps = 1e-10;
  assert(A.noRows() == B.noRows());
  assert(A.noCols() == B.noCols());
  const uint m = A.noRows();
  const uint n = A.noCols();
  for(uint i = 0; i < m; ++i) {
    for(uint j = 0; j < n; ++j) {
      const bool lAiz = fabs(A(i,j)) <= lEps;
      const bool lBiz = fabs(B(i,j)) <= lEps;
      if(lAiz != lBiz) {
        return false;
      }
    }
  }
  return true;
}

// A =
//  ( M -M )
//  ( 0  M )
//
// more specifically, if A_k is 2^k x 2^k complete design matrix for k boolean factors then
// A_k =
//  ( A_{k-1}  -A_{k-1} )
//  (    0      A_{k-1} )
//

bool
isOfFormM_M0M(const Matrix& A) {
  assert(A.noRows() == A.noCols());
  const uint n  = A.noRows();
  const uint n2 = n/2;
  for(uint i = 0; i < n2; ++i) {
    for(uint j = 0; j < n2; ++j) {
      if(A(i,j) != -A(i, n2 + j)) {
        return false;
      }
      if(A(i,j) != A(n2 + i, n2 + j)) {
        return false;
      }
      if(0 != A(n2 + i, j)) {
        return false;
      }
    }
  } 
  return true;
}


//
// (M M)
// (0 M)
//

bool
isOfFormMM0M(const Matrix& A) {
  assert(A.noRows() == A.noCols());
  const uint n  = A.noRows();
  const uint n2 = n/2;
  for(uint i = 0; i < n2; ++i) {
    for(uint j = 0; j < n2; ++j) {
      if(A(i,j) != A(i, n2 + j)) {
        return false;
      }
      if(A(i,j) != A(n2 + i, n2 + j)) {
        return false;
      }
      if(0 != A(n2 + i, j)) {
        return false;
      }
    }
  }
  return true;
}

bool
hasRowSum0ExceptLastRow(const Matrix& A) {
  double lSum = 0;
  for(uint i = 0; i < (A.noRows() - 1); ++i) {
    for(uint j = 0; j < A.noCols(); ++j) {
      lSum += A(i,j);
    }
    if(0 != lSum) {
      return false;
    }
  }
  return true;
}

bool
haveSameZeros(const Matrix& A, const Matrix& B) {
  const uint m = A.noRows();
  const uint n = A.noCols();
  assert(B.noRows() == m);
  assert(B.noCols() == n);
  for(uint i = 0; i < m; ++i) {
    for(uint j = 0; j < n; ++j) {
      const double a = A(i,j);
      const double b = B(i,j);
      if(!((0 == a) == (0 == b))) {
       return false;
      }
    }
  }
  return true;
}

bool
haveSameAbsValue(const Matrix& A, const Matrix& B) {
  const uint m = A.noRows();
  const uint n = A.noCols();
  assert(B.noRows() == m);
  assert(B.noCols() == n);
  for(uint i = 0; i < m; ++i) {
    for(uint j = 0; j < n; ++j) {
      const double a = A(i,j);
      const double b = B(i,j);
      if(fabs(a) != fabs(b)) {
       return false;
      }
    }
  }
  return true;
}


// the inverse of the complete design matrix is symmetric and persymmetric
// the inverse of the complete design matrix contains -1, 0, +1 only
// the row sum of every row except for the last row is 0
//             of the last row is 1


void
test4(const uint aNoBf, const bool aTrace) {
  // const uint n = (1 << aNoBf);
  if(aTrace) {
    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "INVERSE OF COMPLETE DESIGN MATRIX with noBf = " << aNoBf << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
  }

  Matrix A;
  fGetFullDesignMatrix(A, aNoBf);
  if(aTrace) {
    std::cout << "full design matrix:" << std::endl;
    A.printWithRowSum(std::cout, 1);
    std::cout << std::endl;
  }
  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);
  if(aTrace) {
    std::cout << "inverse of full design matrix:" << std::endl;
    Ainv.printWithRowSum(std::cout, 3);
    std::cout << std::endl;
  }
  Matrix AAinv;
  AAinv.setToProductOf(A, Ainv);
  if(aTrace) {
    std::cout << "AA^{-1} = " << std::endl;
    AAinv.print(std::cout, 1);
  }

  std::cout << "@results for noBf = " << aNoBf << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A      is    symmetric: " << A.isSymmetric() << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A      is persymmetric: " << A.isMinorSymmetric() << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A      is 0, 1, -1    : " << isAbs01(A) << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A      is MM0M        : " << isOfFormMM0M(A) << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A      is M-M0M       : " << isOfFormM_M0M(A) << std::endl;

  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A^{-1} is    symmetric: " << Ainv.isSymmetric() << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A^{-1} is persymmetric: " << Ainv.isMinorSymmetric() << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A^{-1} is 0, 1, -1    : " << isAbs01(Ainv) << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A^{-1} is MM0M        : " << isOfFormMM0M(Ainv) << std::endl;
  std::cout << "  @ " << std::setw(2) << aNoBf 
            << " A^{-1} is M-M0M       : " << isOfFormM_M0M(Ainv) << std::endl;
  std::cout << "  @ same zero: " << hasSameZero(A, Ainv) << std::endl;
  std::cout << "  @ same zero: " << haveSameZeros(A, Ainv) << std::endl;
  std::cout << "  @ same absv: " << haveSameAbsValue(A, Ainv) << std::endl;
  std::cout << "  @ is invers: " << isInverse(A, Ainv) << std::endl;
}


// compare test0 src/opt/card/infra/main_lub.cc
void
test5() {
  const uint lNoBf = 3;
  std::cout << std::endl;
  std::cout << "=====================================" << std::endl;
  std::cout << "TEST 5: example test0 from main_lub  " << lNoBf << std::endl;
  std::cout << "=====================================" << std::endl;
  std::cout << std::endl;
  const uint n = (1 << lNoBf);
  const double lCard = 581012;

  std::cout << "|R| = " << lCard << std::endl;

  const uint   lPat[7] = { 0, 1, 2, 3, 4, 5, 6 };
  const double lKs[7]  = { 1,
                           0.022435,
                           0.152923,
                           0.0180633,
                           0.0339976,
                           0.00213937,
                           0.0111753
                         };

  double b[n] = {0};
  double x[n] = {0};
  for(uint i = 0; i < n; ++i) {
    b[i] = 0;
    x[i] = 0;
  }
  for(uint i = 0; i < 7; ++i) {
    b[ lPat[i] ] = round(lCard * lKs[i]);
  }

  Matrix A;
  fGetFullDesignMatrix(A, lNoBf);

  std::cout << "full design matrix:" << std::endl;
  A.printWithRowSum(std::cout, 1);
  std::cout << std::endl;

  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);

  std::cout << "inverse of full design matrix:" << std::endl;
  Ainv.printWithRowSum(std::cout, 3);
  std::cout << std::endl;


  std::cout << " b = ";
  Matrix::printVector(std::cout, b, n);
  std::cout << std::endl;

  Ainv.getAx(x, b);

  double lSumX = 0;
  for(uint i = 0; i < n; ++i) {
    lSumX += x[i];
  }

  std::cout << " x = ";
  Matrix::printVector(std::cout, x, n);
  std::cout << "  sum x = " << lSumX << std::endl;

  double Ax[n];
  A.getAx(Ax, x);
  std::cout << "Ax = ";
  Matrix::printVector(std::cout, Ax, n);
  std::cout << std::endl;

  double x0[n];
  double xm[n];
  for(uint i = 0; i < n; ++i) {
    x0[i] = x[i];
  }

  std::cout << "LOOP:" << std::endl;
  bool lIsPositive = true;
  for(double blast = 1240; (blast < 1250) && lIsPositive; blast += 1) {
    for(uint i = 0; i < n; ++i) {
      xm[i] = x[i];
    }
    b[n-1] = blast;
    std::cout << "b = ";
    Matrix::printVector(std::cout, b, n) << std::endl;
    std::cout << "x = ";
    Ainv.getAx(x, b);
    Matrix::printVector(std::cout, x, n) << std::endl;
    std::cout << std::endl;
    for(uint i = 0; i < n; ++i) {
     if(0 > x[i]) {
       lIsPositive = false;
       break;
     }
    }
  }
  std::cout << "END LOOP" << std::endl;
  std::cout << std::endl;
  std::cout << "x_0 = ";
  Matrix::printVector(std::cout, x0, n) << std::endl;
  std::cout << "x_m = ";
  Matrix::printVector(std::cout, xm, n) << std::endl;
  double b0[n];
  double bm[n];
  A.getAx(b0, x0);
  A.getAx(bm, xm);
  std::cout << "b_0 = ";
  Matrix::printVector(std::cout, b0, n) << std::endl;
  std::cout << "b_m = ";
  Matrix::printVector(std::cout, bm, n) << std::endl;

  std::cout << std::endl;
}

void
test6() {
  const uint aNoBf = 2;
  double x[4] = {0, 0, 0, 0};
  double b[4] = {10, 0, 0, 0};
  Matrix A;
  fGetFullDesignMatrix(A, aNoBf);
  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);


  for(double b1 = 0; b1 < 10; ++b1) {
    b[1] = b1;
    for(double b2 = 0; b2 < 10; ++b2) {
      b[2] = b2;
      for(double b3 = 0; b3 < 10; ++b3) {
        b[3] = b3;
        Ainv.getAx(x, b);
        std::cout << "b = "; Matrix::printVector(std::cout, b, 4, 3) << std::endl;
        std::cout << "x = "; Matrix::printVector(std::cout, x, 4, 3) << std::endl;
      }
    }
  }
 
}


uint32_t
fCheckAllMpi(const Matrix& A, const Matrix& Ainv, const uint_vt aRowsToRetain, const uint z, const bool aTrace) {
  std::cout << "MPI enter check mpi props (z = " << z << ')' << std::endl;

  Matrix B;
  B.setToOnlyRows(A, aRowsToRetain);
  Matrix Binv;
  Binv.setToOnlyCols(Ainv, aRowsToRetain);

  if(aTrace) {
    std::cout << "to retain: " << aRowsToRetain << std::endl;
    // std::cout << "B = " << std::endl; B.print(std::cout, 1) << std::endl;
    // std::cout << "B^{-1} =" << std::endl; Binv.print(std::cout, 2) << std::endl;
  }

  const uint32_t lResult = fCheckMpiProps(B, Binv, aTrace);


  std::cout << "MPI result check mpi props: ";
  Bitvector32 lBvResult(lResult);
  lBvResult.print(std::cout, 10)  << std::endl;

  return lResult;
}

uint32_t
test7(const uint aNoBf, const bool aTrace) {
  const uint z = aNoBf;
  const uint n = (1 << aNoBf);
  std::cout << std::endl;
  std::cout << "=====================================" << std::endl;
  std::cout << "TEST 7: test Properties Of A_" << z << "|r(T)  " << std::endl;
  std::cout << "=====================================" << std::endl;
  std::cout << std::endl;

  assert(6 >= z);  // z == 6 runs for ever!
  Matrix A;
  fGetFullDesignMatrix(A, aNoBf);
  Matrix Ainv;
  Ainv.setToInverseOfUpperTriangular(A, 1E-10);

  if(aTrace) {
    // std::cout << "A = " << std::endl; A.print(std::cout, 1) << std::endl;
    // std::cout << "A^{-1} =" << std::endl; Ainv.print(std::cout, 2) << std::endl;
  }

  uint32_t lRes = ~((uint32_t) 0);
  uint_vt lRowsToRetain;
  for(uint64_t i = 0; i < ((uint64_t) 1 << (n - 1)); ++i) {
    lRowsToRetain.clear();
    for(BvMemberIdxAsc<uint64_t> lIter((uint64_t) 1 | (i << 1)); lIter.isValid(); ++lIter) {
      lRowsToRetain.push_back(*lIter);
    }
    const uint lRes1 = fCheckAllMpi(A, Ainv, lRowsToRetain, z, aTrace);
    lRes &= lRes1;
    if((0 < i) && (0 == (i % 1000000))) {
      std::cout << "@MPI count = " << i << "  lRes = ";
      Bitvector32(lRes).print(std::cout, 12) << std::endl;
    }
  }
  return lRes;
}

void
fPrintSum(const uint aNoBf) {
  const uint N = (1 << aNoBf);
  for(uint i = 0; i < N; ++i) {
    for(uint j = 0; j < N; ++j) {
      if(0 < j) {
        std::cout << " & ";
      }
      if(j == (i | j)) {
        std::cout << "\\gamma_{" << j << "}";
      } else {
        std::cout << "          ";
      }
    }
    std::cout << " &=& " << "\\beta_{" << i << "} \\\\" << std::endl;
  }
}

// the number of '+' operations needed to calculate Cx,
// C complete design matrix
uint64_t
fNoAddOpForCx(const uint aNoBf) {
  const uint64_t N = ((uint64_t) 1 << aNoBf);
  const uint64_t lMask = (N - 1);
  uint64_t lNoOp = 0;
  for(uint64_t i = 0; i < N; ++i) {
     lNoOp += ((uint64_t) 1 << number_of_bits_set(~i & lMask)) - 1; // #1 in superset - 1
  }
  return lNoOp;
}

uint64_t
fNoAddOpForCxFactorized(const uint z) {
  if(0 == z) {
    return 0;
  } else
  if(1 == z) {
    return 1;
  }
  return (2 * fNoAddOpForCxFactorized(z-1)) + ((uint64_t) 1 << (z - 1));
}

uint64_t
fNoAddOpForCxFactorizedClosed(const uint z) {
  return z*(1 << (z-1));
}

// compare the number of add operations needed to calculate
//  Cx
// where C is the complete design matrix and x some vector
void
testNoAddOpForCx() {
  std::cout << "#add-op" << std::endl;
  for(uint z = 1; z <= 20; ++z) {
    const uint64_t lRaw  = fNoAddOpForCx(z);
    const uint64_t lFac  = fNoAddOpForCxFactorized(z);
    const uint64_t lFacC = fNoAddOpForCxFactorizedClosed(z);
    const double lRat = ((double) lRaw / (double) lFac);
    // const uint64_t lRat = (lRaw / lFac);
    std::cout << std::setw(2) << z << "  " 
              << std::setw(12) << lRaw << "  "
              << std::setw(12) << lFac << "  "
              << std::setw(12) << lFacC << "  "
              << std::setw(12) << mt::roundXt<double>(lRat)
              << std::endl;
  }
}

// compare Horn, Johnson lemma 8.4.1 p 507:
// A \geq 0 irreducible if and only if (I + A)^{n-1} > 0
//
// comment for design matrix:
// A upper triangular --> (I + A^n) upper triangular
// --> not irreducible if (A^n >= 0)
bool
fCheckIrreducible(const uint n) {
  const uint N = (1 << n);
  Matrix C;
  fGetFullDesignMatrix(C, n);

  Matrix X(N,N);
  Matrix D(C);
  for(uint i = 0; i < N; ++i) {
    D(i,i) += 1;
  }
  std::cout << "(I + C) = " << std::endl;
  D.print(std::cout, 3) << std::endl;

  for(uint r = 2; r < N; ++r) {
    X.setToProductOf(D,C);
    D = X;
    std::cout << "(I + C)^{" << r << "} = " << std::endl;
    D.print(std::cout, 5) << std::endl;
  }

  bool lRes = true;
  for(uint i = 0; i < N; ++i) {
    for(uint j = 0; j < N; ++j) {
      if(D(i,j) <= 0) {
        lRes = false;
        break;
      }
    }
  }
  if(lRes) {
    std::cout << "C_{" << n << "} is irreducible." << std::endl;
  } else {
    std::cout << "C_{" << n << "} is not irreducible." << std::endl;
  }
  return lRes;
}

double
quadratic_form(const double_vt& x, const Matrix& A) {
  assert(x.size() == A.noRows());
  assert(x.size() == A.noCols());
  const uint n = x.size();
  double_vt y(n);
  A.getAx(y.data(), x.data());
  double lRes = 0;
  for(uint i = 0; i < n; ++i) {
    lRes += x[i] * y[i];
  }
  return lRes;
}

void
fCheckQuadraticForm() {
  std::cout << "fCheckQuadraticForm:" << std::endl;
  const uint n = 1;
  Matrix C;
  fGetFullDesignMatrix(C, n);
  std::cout << "C = " << std::endl;
  C.print(std::cout, 3) << std::endl;

  Matrix Cinv;
  Cinv.setToInverseOfUpperTriangular(C, 1E-16);
  std::cout << "C^{-1} = " << std::endl;
  Cinv.print(std::cout, 3) << std::endl;

  double_vt x(2);
  for(double x1 = -1; x1 <= +1; x1 += 0.001) {
    for(double x2 = -1; x2 <= +1; x2 += 0.001) {
      x[0] = x1;
      x[1] = x2;
      const double q_C    = quadratic_form(x,C);
      const double q_Cinv = quadratic_form(x,Cinv);
      if(0 > q_C) {
        std::cout << " q_C(" << x << ") = " << q_C << std::endl;
      }
      if(0 > q_Cinv) {
        std::cout << " q_Cinv(" << x << ") = " << q_Cinv << std::endl;
      }
      assert(0 <= q_C);
      assert(0 <= q_Cinv);
    }
  }

  std::cout << "C_1      is positive definite." << std::endl;
  std::cout << "C_1^{-1} is positive definite." << std::endl;
}

int
main(const int argc, const char* argv[]) {
  arg_t lArg;
  const bool lOk = parse_args(lArg, argc, argv);
  if(!lOk) {
    std::cerr << "usage: " << argv[0] << " [-f <f>] <n> : format f in [1,7], no Bf n in [2, 10]" << std::endl;
    return -1;
  }

  // test_1(3);
  // test_2();

  // test_3();

/*
  test0(lArg);

  // testLDL();

  test1(lArg);

  test2(2);
  std::cout << std::endl;
  test2(3);
  std::cout << std::endl;
  test2(4);
  std::cout << std::endl;

  test3( 2, true);
  test3( 3, true);
  test3( 4, true);
  // test3( 5, true);
  // test3( 6, false);
  // test3( 7, false);
  // test3( 8, false);
  // test3( 9, false);
  // test3(11, false);
  // test3(12, false);
  // test3(13, false);
  // test3(14, false);
  // test3(15, false);
*/

/*  
  for(int i = 2; i < 11; ++i) {
    test4(i, (i < 5));
  }
*/

  // test5();


  // test6();

  // test_4();

  // test_5(2);
  // test_5(3);


  fPrintFullDesignMatrix(lArg);

  fTestGamma2Beta(lArg);

/*
  for(int i = 2; i < 6; ++i) {
    const uint lRes = test7(i, (i < 4));
    std::cout << "@MPI z = " << i << ": ";
    Bitvector32(lRes).print(std::cout, 12) << std::endl;
  }
*/

  fPrintSum(3);

  testNoAddOpForCx();

  fCheckIrreducible(3);
  fCheckQuadraticForm();

  return 0;
}



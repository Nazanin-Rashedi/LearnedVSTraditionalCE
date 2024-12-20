#ifndef SRC_INFRA_ZCURVE_HH
#define SRC_INFRA_ZCURVE_HH

#include <inttypes.h>
// #include <immintrin.h>
#include <infra/bit_intrinsics.hh>

/* compile with
g++ -std=c++11 -mbmi2 -O3 main_zcurve.cc
*/

template<typename Tuint>
class ZCurve {
  public:
    typedef unsigned int uint;

    struct lohi_t {
      Tuint lo;
      Tuint hi;

      lohi_t() : lo(0), hi(0) {}
      lohi_t(const Tuint aLo, const Tuint aHi) : lo(aLo), hi(aHi) {}
    };

    typedef void  (*cbfun_t)(const lohi_t&); // arg: range on z-curve
    typedef Tuint (*cbjumpfun_t)(const lohi_t&); // arg: range on z-curve, returning a value on z-curve to restart on


    struct recdesc_t {
      lohi_t   d;
      uint32_t xlo;
      uint32_t ylo;
      uint32_t xhi;
      uint32_t yhi;

      recdesc_t(const uint32_t aXlo, const uint32_t aYlo,
                const uint32_t aXhi, const uint32_t aYhi,
                const uint32_t aNoBits)
               : d(), xlo(aXlo), ylo(aYlo), xhi(aXhi), yhi(aYhi) {
        ZCurve<Tuint> lZCurve(aNoBits);
        d.lo = lZCurve.xy2d(aXlo, aYlo);
        d.hi = lZCurve.xy2d(aXhi, aYhi);
      }

      recdesc_t(const lohi_t& aLohi, const uint32_t aNoBits) : d(aLohi), xlo(), ylo(), xhi(), yhi() {
        ZCurve<Tuint> lZCurve(aNoBits);
        lZCurve.d2xy(d.lo, xlo, ylo);
        lZCurve.d2xy(d.hi, xhi, yhi);
      }

      recdesc_t(const Tuint aDlo, const Tuint aDhi, const uint32_t aNoBits) : d(aDlo, aDhi), xlo(), ylo(), xhi(), yhi() {
        ZCurve<Tuint> lZCurve(aNoBits);
        lZCurve.d2xy(d.lo, xlo, ylo);
        lZCurve.d2xy(d.hi, xhi, yhi);
      }
    };
  public:
    ZCurve(const uint n); // n: aNumberOfBits per direction, at most sizeof(TUint)*8/2
  public:

   // must have Tcallback::processFragment()
    template<class Tcallback>
    Tuint listFragments(const recdesc_t&, Tcallback&, const bool aTrace) const; 

    // Tcallback::processFragmentJump() must return some new point (Tuint) on the z-curve
    template<class Tcallback>
    Tuint listFragmentsJump(const recdesc_t&, Tcallback&, const bool aTrace) const; 

  public:
    inline uint  noBits() const { return _noBits; }
    inline Tuint mask() const { return _mask; }
    inline Tuint maskX() const { return _maskX; }
    inline Tuint maskY() const { return _maskY; }
  public:
    inline Tuint get_n() const { return (((Tuint) 1) << noBits()); } // number of values per direction
    inline Tuint getMaxD() const { return (get_n() * get_n() - 1); } // max. (last) value on z-curve
  public:
    inline Tuint xy2d(const Tuint x, const Tuint y) const;
    inline void  d2xy(const Tuint d, Tuint& x, Tuint& y) const;
  public:
    inline static Tuint    mkBit(const uint32_t aLevel) { return ((Tuint) 1 << aLevel); } // to be used in mkLeft, mkRight
    inline static Tuint    mkLeft (const Tuint aHi, const Tuint aBit) { return (aHi & ~(aBit)); }
    inline static Tuint    mkRight(const Tuint aLo, const Tuint aBit) { return (aLo |  (aBit)); }
  private:
    template<class Tcallback>
    inline Tuint listFragmentsSub(const recdesc_t& aQueryRectangle, // the query rectangle
                                  const lohi_t&    aFragment,       // fragment of the z-curve
                                  const uint32_t   aLevel,          // level: top: 2*noBits - 1
                                  Tcallback&       aCallback,       // callback class must have method processFragment
                                  const bool       aTrace,          // do trace?
                                  const uint32_t   aIndent) const;  // only for tracing
  private:
    uint  _noBits;
    Tuint _mask; // mask for (x,y) coordinates
  private:
    static const Tuint _maskX = (Tuint) 0x5555555555555555LL; // selector matrix for X coordinate
    static const Tuint _maskY = (Tuint) 0xAAAAAAAAAAAAAAAALL; // selector matrix for Y coordinate
};

template<typename Tuint>
ZCurve<Tuint>::ZCurve(const uint aNoBits) : _noBits(aNoBits), _mask((((Tuint) 1) << aNoBits) -1 ) {
}


template<>
inline uint32_t
ZCurve<uint32_t>::xy2d(const uint32_t x, const uint32_t y) const {
  // return (_pdep_u32(x & mask(), _maskX) | _pdep_u32(y & mask(), _maskY));
  return (bit_distribute<uint32_t>(x & mask(), _maskX) | 
          bit_distribute<uint32_t>(y & mask(), _maskY));
}

template<>
inline uint64_t
ZCurve<uint64_t>::xy2d(const uint64_t x, const uint64_t y) const {
  // return (_pdep_u64(x, _maskX) | _pdep_u64(y, _maskY));
  return (bit_distribute<uint64_t>(x & mask(), _maskX) | 
          bit_distribute<uint64_t>(y & mask(), _maskY));
}

template<>
inline void
ZCurve<uint32_t>::d2xy(const uint32_t d, uint32_t& x, uint32_t& y) const {
  // x = _pext_u32(d, _maskX);
  // y = _pext_u32(d, _maskY);
  x = bit_gather<uint32_t>(d, _maskX);
  y = bit_gather<uint32_t>(d, _maskY);
}

template<>
inline void
ZCurve<uint64_t>::d2xy(const uint64_t d, uint64_t& x, uint64_t& y) const {
  // x = _pext_u64(d, _maskX);
  // y = _pext_u64(d, _maskY);
  x = bit_gather<uint64_t>(d, _maskX);
  y = bit_gather<uint64_t>(d, _maskY);
}


template<typename Tuint>
std::ostream&
operator<<(std::ostream& os, const typename ZCurve<Tuint>::lohi_t& x) {
  os << '[' << x.lo << ',' << x.hi << ']';
  return os;
}

template<typename Tuint>
std::ostream&
operator<<(std::ostream os, const typename ZCurve<Tuint>::recdesc_t& x) {
    os << '['
     << x.xlo << ',' << x.ylo << ':' << x.xhi << ',' << x.yhi << ';'
     << x.d.lo << ',' << x.d.hi
     << ']';
  return os;
}

template<typename Tuint>
template<class Tcallback>
Tuint
ZCurve<Tuint>::listFragments(const recdesc_t& aQueryRectangle, Tcallback& aCallback, const bool aTrace) const {
  const Tuint  n     = get_n();
  const Tuint  lMaxD = (n*n) - 1;
  const lohi_t lFragment(0, lMaxD);
  return listFragmentsSub(aQueryRectangle,
                          lFragment,
                          2 * noBits() - 1,
                          aCallback,
                          false,
                          3);
}

template<typename Tuint>
template<class Tcallback>
Tuint
ZCurve<Tuint>::listFragmentsSub(const recdesc_t& aQueryRectangle,
                                const lohi_t&    aFragment,
                                const uint       aLevel,
                                      Tcallback& aCallback,
                                const bool       aTrace,
                                const uint       aIndent) const {
  const Tuint n = (((Tuint) 1) << noBits());
  if(aTrace) {
    std::cout << "fla B = " << noBits() << " L = " << aLevel 
              << " n = " << n
              << std::endl;
  }

  Tuint lCount = 1;
   if(aQueryRectangle.d.hi < aFragment.lo) {
    // empty a
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty a"
                << std::endl;
    }
    return lCount;
  }
  if(aFragment.hi < aQueryRectangle.d.lo) {
    // empty b
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty b"
                << std::endl;
    }
    return lCount;
  }

  // still everything possible
  uint32_t lXlo = 0;
  uint32_t lYlo = 0;
  uint32_t lXhi = 0;
  uint32_t lYhi = 0;
  d2xy(aFragment.lo, lXlo, lYlo);
  d2xy(aFragment.hi, lXhi, lYhi);

  if(aTrace) {
    std::cout << "xlo = " << lXlo << ", "
              << "ylo = " << lYlo << ", "
              << "xhi = " << lXhi << ", "
              << "yhi = " << lYhi;
  }

   if(lXhi < aQueryRectangle.xlo) {
    // empty c
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty c"
                << std::endl;
    }
    return lCount;
  }

  if(lYhi < aQueryRectangle.ylo) {
    // empty d
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty d"
                << std::endl;
    }
    return lCount;
  }

  if(lXlo > aQueryRectangle.xhi) {
    // empty e
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty e"
                << std::endl;
    }
    return lCount;
  }

  if(lYlo > aQueryRectangle.yhi) {
    // empty f
    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : empty f"
                << std::endl;
    }
    return lCount;
  }

  const bool lContained = ((aQueryRectangle.xlo <= lXlo) && (aQueryRectangle.ylo <= lYlo) &&
                           (lXhi <= aQueryRectangle.xhi) && (lYhi <= aQueryRectangle.yhi));

  if(aTrace) {
    std::cout << ": contained = " << lContained << std::endl;
  }

  if(lContained) {
    // containment a \subseteq q
    ++lCount;

    aCallback.processFragment(aFragment);

    if(aTrace) {
      std::cout << std::string(aIndent, ' ')
                << "q " // << aQueryRectangle << " a " << aFragment
                << " : containment"
                << std::endl;
    }
    return lCount;
  }

  // overlapping
  const uint32_t lTheBit = (1 << aLevel);
  const uint32_t lLeft  = mkLeft(aFragment.hi, lTheBit);  // (aHi & ~(lTheBit)); // alternative
  const uint32_t lRight = mkRight(aFragment.lo, lTheBit); // (aLo | (lTheBit));  // alternative
  lohi_t lFragmentLeft (aFragment.lo, lLeft);
  lohi_t lFragmentRight(lRight, aFragment.hi);

  if(aTrace) {
    std::cout << std::string(aIndent, ' ')
              << "q " // << aQueryRectangle << " a " << aFragment
              << " : overlapping"
              << std::endl;
  }

  lCount += listFragmentsSub(aQueryRectangle, lFragmentLeft,  aLevel - 1, aCallback, aTrace, aIndent + 3);
  lCount += listFragmentsSub(aQueryRectangle, lFragmentRight, aLevel - 1, aCallback, aTrace, aIndent + 3);
  return lCount;



}





#endif

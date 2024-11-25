#include "Sampling_NRAlphaOmega.hh"
namespace H2D{

	Sampling_NRAlphaOmega::Sampling_NRAlphaOmega(const Data2dim& aData,
                       const uint      aSampleSize,
                       const double    aQ,
                       const double    aTheta,
                       const bool      aTrace,
		       const double aEps)
		:Sample2dim(aData,aSampleSize,aQ,aTheta,aTrace),
		_aEps(aEps),
		_alphaSampler(SamplingAlphaOmegaNewton(aData.total(),
					aSampleSize,aEps))

{
//std::cout<<samples().size()<<std::endl;

}
double Sampling_NRAlphaOmega::estimate(const query_t& lQuery) const{
	const uint64_t k =(uint64_t) samples().countWithin(lQuery.rectangle());
	//std::cout<<_data.size()<<std::endl;
	if((double)k / _alphaSampler.m() < 0.5){
	       	SamplingAlphaOmegaNewton alphaSamplerCopy = _alphaSampler;
	        double alpha = alphaSamplerCopy.alpha(k);
	        double omega = alphaSamplerCopy.omega(k);
//	std::cout<<"k: "<<k<<" m: "<<_alphaSampler.m()<<" , alpha: "<<alpha<<" , Omega: "<<omega<<" ,estimate: "<<std::max<double>(minEstimate(),std::round(sqrt(alpha*omega)))<<"Nazanin: "<<(omega - alpha)/std::log(omega / std::max<double>(1,alpha))<<" ,cardinality:"<<lQuery.card()<<", if simple: "<<std::max<double>(minEstimate(),round((double) k * ((double) dataSize() / (double) sampleSize())))<<std::endl;

		if((omega / alpha) > 100)
		{
			return std::max<double>(k,round((omega - alpha)/std::log(omega / std::max<double>(1,alpha))));
		}
		else{
			return std::max<double>(k,round(std::sqrt(alpha*omega)));
		}	

		}
		else {
			return std::max<double>(minEstimate(),round((double) k * ((double) dataSize() / (double) sampleSize())));
			std::cout<<"estimate: "<<std::max<double>(k,round((double) k * ((double) dataSize() / (double) sampleSize())))<<" ,cardinality: "<<lQuery.card()<<std::endl;
	

		}

}


Sampling_NRAlphaOmega::~Sampling_NRAlphaOmega(){
}
}


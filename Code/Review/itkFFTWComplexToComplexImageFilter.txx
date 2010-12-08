/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef __itkFFTWComplexToComplexImageFilter_txx
#define __itkFFTWComplexToComplexImageFilter_txx

#include "itkFFTWComplexToComplexImageFilter.h"
#include "itkFFTComplexToComplexImageFilter.txx"
#include <iostream>
#include "itkIndent.h"
#include "itkMetaDataObject.h"
#include "itkImageRegionIterator.h"
#include "itkProgressReporter.h"

namespace itk
{

template< typename TPixel, unsigned int VDimension >
void
FFTWComplexToComplexImageFilter< TPixel, VDimension >::
BeforeThreadedGenerateData()
{
  // get pointers to the input and output
  typename InputImageType::ConstPointer inputPtr  = this->GetInput();
  typename OutputImageType::Pointer outputPtr = this->GetOutput();

  if ( !inputPtr || !outputPtr )
    {
    return;
    }

  // we don't have a nice progress to report, but at least this simple line
  // reports the begining and the end of the process
  ProgressReporter progress(this, 0, 1);

  // allocate output buffer memory
  outputPtr->SetBufferedRegion( outputPtr->GetRequestedRegion() );
  outputPtr->Allocate();

  const typename InputImageType::SizeType &   outputSize =
    outputPtr->GetLargestPossibleRegion().GetSize();
  const typename OutputImageType::SizeType & inputSize =
    inputPtr->GetLargestPossibleRegion().GetSize();

  // figure out sizes
  // size of input and output aren't the same which is handled in the superclass,
  // sort of.
  // the input size and output size only differ in the fastest moving dimension
  unsigned int total_outputSize = 1;
  unsigned int total_inputSize = 1;

  for ( unsigned i = 0; i < VDimension; i++ )
    {
    total_outputSize *= outputSize[i];
    total_inputSize *= inputSize[i];
    }

  int transformDirection = 1;
  if ( this->GetTransformDirection() == Superclass::INVERSE )
    {
    transformDirection = -1;
    }

  typename FFTWProxyType::PlanType plan;
  typename FFTWProxyType::ComplexType * in = (typename FFTWProxyType::ComplexType*) inputPtr->GetBufferPointer();
  typename FFTWProxyType::ComplexType * out = (typename FFTWProxyType::ComplexType*) outputPtr->GetBufferPointer();
  int flags = m_PlanRigor;
  if( !m_CanUseDestructiveAlgorithm )
    {
    // if the input is about to be destroyed, there is no need to force fftw
    // to use an non destructive algorithm. If it is not released however,
    // we must be careful to not destroy it.
    flags = flags | FFTW_PRESERVE_INPUT;
    }
  switch(VDimension)
    {
    case 1:
      plan = FFTWProxyType::Plan_dft_1d(inputSize[0],
                                           in,
                                           out,
                                           transformDirection,
                                           flags,
                                           this->GetNumberOfThreads());
      break;
    case 2:
      plan = FFTWProxyType::Plan_dft_2d(inputSize[1],
                                           inputSize[0],
                                           in,
                                           out,
                                           transformDirection,
                                           flags,
                                           this->GetNumberOfThreads());
      break;
    case 3:
      plan = FFTWProxyType::Plan_dft_3d(inputSize[2],
                                           inputSize[1],
                                           inputSize[0],
                                           in,
                                           out,
                                           transformDirection,
                                           flags,
                                           this->GetNumberOfThreads());
      break;
    default:
      int *sizes = new int[VDimension];
      for(unsigned int i = 0; i < VDimension; i++)
        {
        sizes[(VDimension - 1) - i] = inputSize[i];
        }

      plan = FFTWProxyType::Plan_dft(VDimension,sizes,
                                        in,
                                        out,
                                        transformDirection,
                                        flags,
                                        this->GetNumberOfThreads());
      delete [] sizes;
    }
  FFTWProxyType::Execute(plan);
  FFTWProxyType::DestroyPlan(plan);
}

template <typename TPixel, unsigned int VDimension>
void
FFTWComplexToComplexImageFilter< TPixel, VDimension >::
ThreadedGenerateData(const OutputImageRegionType& outputRegionForThread, int threadId )
{
  //
  // Normalize the output if backward transform
  //
  if ( this->GetTransformDirection() == Superclass::INVERSE )
    {
    typedef ImageRegionIterator< OutputImageType >   IteratorType;
    unsigned long total_outputSize = this->GetOutput()->GetRequestedRegion().GetNumberOfPixels();
    IteratorType it(this->GetOutput(), outputRegionForThread);
    while( !it.IsAtEnd() )
      {
      std::complex< TPixel > val = it.Value();
      val /= total_outputSize;
      it.Set(val);
      ++it;
      }
    }
}

template< typename TPixel, unsigned int VDimension >
bool
FFTWComplexToComplexImageFilter< TPixel, VDimension >::FullMatrix()
{
  return false;
}


template< typename TPixel, unsigned int VDimension >
void
FFTWComplexToComplexImageFilter< TPixel, VDimension >::
UpdateOutputData(DataObject * output)
{
  // we need to catch that information now, because it is changed later
  // during the pipeline execution, and thus can't be grabbed in
  // GenerateData().
  m_CanUseDestructiveAlgorithm = this->GetInput()->GetReleaseDataFlag();
  Superclass::UpdateOutputData( output );
}

template< typename TPixel, unsigned int VDimension >
void
FFTWComplexToComplexImageFilter< TPixel, VDimension >
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "PlanRigor: " << FFTWGlobalConfiguration::GetPlanRigorName(m_PlanRigor) << " (" << m_PlanRigor << ")" << std::endl;
}

} // namespace itk
#endif // _itkFFTWComplexToComplexImageFilter_txx

/*=========================================================================
 *
 *  Copyright RTK Consortium
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
#ifndef __rtkInterpolatorWithKnownWeightsImageFilter_txx
#define __rtkInterpolatorWithKnownWeightsImageFilter_txx

#include "rtkInterpolatorWithKnownWeightsImageFilter.h"

#include "itkObjectFactory.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"

namespace rtk
{

template< typename VolumeType, typename VolumeSeriesType>
InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>::InterpolatorWithKnownWeightsImageFilter()
{
  this->SetNumberOfRequiredInputs(2);
  m_ProjectionNumber = 0;
}

template< typename VolumeType, typename VolumeSeriesType>
void InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>::SetInputVolume(const VolumeType* Volume)
{
  this->SetNthInput(0, const_cast<VolumeType*>(Volume));
}

template< typename VolumeType, typename VolumeSeriesType>
void InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>::SetInputVolumeSeries(const VolumeSeriesType* VolumeSeries)
{
  this->SetNthInput(1, const_cast<VolumeSeriesType*>(VolumeSeries));
}

template< typename VolumeType, typename VolumeSeriesType>
typename VolumeType::ConstPointer InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>::GetInputVolume()
{
  return static_cast< const VolumeType * >
          ( this->itk::ProcessObject::GetInput(0) );
}

template< typename VolumeType, typename VolumeSeriesType>
typename VolumeSeriesType::Pointer InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>::GetInputVolumeSeries()
{
  return static_cast< VolumeSeriesType * >
          ( this->itk::ProcessObject::GetInput(1) );
}

template< typename VolumeType, typename VolumeSeriesType>
void InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>
::GenerateInputRequestedRegion()
{
  // Call the superclass implementation
  Superclass::GenerateInputRequestedRegion();

  // Set the requested region of the first input to that of the output
  typename VolumeType::Pointer inputPtr0 = const_cast< VolumeType* >( this->GetInputVolume().GetPointer() );
  if ( !inputPtr0 )
    return;
  inputPtr0->SetRequestedRegion(this->GetOutput()->GetRequestedRegion());

  // Compute the smallest possible requested region of the second input
  // Note that for some projections, the requested frames are 0 and n-1,
  // which implies that the whole input is loaded
  typename VolumeSeriesType::Pointer inputPtr1 = const_cast< VolumeSeriesType* >( this->GetInputVolumeSeries().GetPointer() );
  if ( !inputPtr1 )
    return;

  // Find the minimum and maximum indices of the required frames
  // Initialize the minimum to the highest possible value, and the
  // maximum to the lowest possible value
  typename VolumeSeriesType::IndexValueType maxi =
      inputPtr1->GetLargestPossibleRegion().GetIndex(VolumeSeriesType::ImageDimension - 1);
  typename VolumeSeriesType::IndexValueType mini =
      mini + inputPtr1->GetLargestPossibleRegion().GetSize(VolumeSeriesType::ImageDimension - 1);
  for (unsigned int frame=0; frame<m_Weights.rows(); frame++)
    {
    if (m_Weights[frame][m_ProjectionNumber] != 0)
      {
      if (frame < mini) mini = frame;
      if (frame > maxi) maxi = frame;
      }
    }
  typename VolumeSeriesType::RegionType requested = inputPtr1->GetLargestPossibleRegion();
  for (unsigned int dim=0; dim < VolumeSeriesType::ImageDimension - 1; ++dim)
    {
    requested.SetSize(dim, this->GetOutput()->GetRequestedRegion().GetSize(dim));
    requested.SetIndex(dim, this->GetOutput()->GetRequestedRegion().GetIndex(dim));
    }
  requested.SetSize(VolumeSeriesType::ImageDimension - 1, maxi - mini + 1);
  requested.SetIndex(VolumeSeriesType::ImageDimension - 1, mini);

  // Set the requested region
  inputPtr1->SetRequestedRegion(requested);
}

template< typename VolumeType, typename VolumeSeriesType>
void InterpolatorWithKnownWeightsImageFilter<VolumeType, VolumeSeriesType>
::ThreadedGenerateData(const typename VolumeType::RegionType& outputRegionForThread, ThreadIdType itkNotUsed(threadId) )
{
  typename VolumeType::ConstPointer volume = this->GetInputVolume();
  typename VolumeSeriesType::Pointer volumeSeries = this->GetInputVolumeSeries();

  unsigned int Dimension = volume->GetImageDimension();

  typename VolumeSeriesType::RegionType volumeSeriesRegion;
  typename VolumeSeriesType::SizeType volumeSeriesSize;
  typename VolumeSeriesType::IndexType volumeSeriesIndex;

  typedef itk::ImageRegionIterator<VolumeType>        VolumeRegionIterator;
  typedef itk::ImageRegionConstIterator<VolumeType>   VolumeRegionConstIterator;
  typedef itk::ImageRegionIterator<VolumeSeriesType>  VolumeSeriesRegionIterator;

  float weight;

  // Initialize output region with input region in case the filter is not in
  // place
  VolumeRegionIterator        itOut(this->GetOutput(), outputRegionForThread);
  if(this->GetInput() != this->GetOutput() )
    {
    VolumeRegionConstIterator   itIn(volume, outputRegionForThread);
    while(!itOut.IsAtEnd())
      {
      itOut.Set(itIn.Get());
      ++itOut;
      ++itIn;
      }
    }

  // Compute the weighted sum of frames (with known weights) to get the output
  for (unsigned int frame=0; frame<m_Weights.rows(); frame++)
    {
    weight = m_Weights[frame][m_ProjectionNumber];
    if (weight != 0)
      {
      volumeSeriesRegion = volumeSeries->GetRequestedRegion();
      volumeSeriesSize = volumeSeriesRegion.GetSize();
      volumeSeriesIndex = volumeSeriesRegion.GetIndex();

      typename VolumeType::SizeType outputRegionSize = outputRegionForThread.GetSize();
      typename VolumeType::IndexType outputRegionIndex = outputRegionForThread.GetIndex();

      for (unsigned int i=0; i<Dimension; i++)
        {
        volumeSeriesSize[i] = outputRegionSize[i];
        volumeSeriesIndex[i] = outputRegionIndex[i];
        }
      volumeSeriesSize[Dimension] = 1;
      volumeSeriesIndex[Dimension] = frame;
      
      volumeSeriesRegion.SetSize(volumeSeriesSize);
      volumeSeriesRegion.SetIndex(volumeSeriesIndex);

      // Iterators
      VolumeSeriesRegionIterator  itVolumeSeries(volumeSeries, volumeSeriesRegion);
      itOut.GoToBegin();

      while(!itOut.IsAtEnd())
        {
        itOut.Set(itOut.Get() + weight * itVolumeSeries.Get());
        ++itVolumeSeries;
        ++itOut;
        }
      }
    }
}

}// end namespace


#endif

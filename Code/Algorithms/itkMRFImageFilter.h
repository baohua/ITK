/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkMRFImageFilter.h
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Insight Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef _itkMRFImageFilter_h
#define _itkMRFImageFilter_h


#include "vnl/vnl_vector.h"
#include "vnl/vnl_matrix.h"

#include "itkImageToImageFilter.h"
#include "itkSupervisedClassifier.h"
#include "itkImageRegionIterator.h"

#include "itkConstNeighborhoodIterator.h"
#include "itkNeighborhoodIterator.h"
#include "itkNeighborhoodAlgorithm.h"
#include "itkNeighborhood.h"
#include "itkSize.h"

namespace itk
{

/** \class MRFImageFilter
 * \brief Implementation of a labeller object that uses Markov Random Fields
 * to classify pixels in an image data set.
 * 
 * This object classifies pixels based on a Markov Random Field (MRF) 
 * model.This implementation uses the maximum a posteriori (MAP) estimates
 * for modeling the MRF. The object traverses the data set and uses the model 
 * generated by the Mahalanobis distance classifier to gets the the distance  
 * between each pixel in the data set to a set of known classes, updates the 
 * distances by evaluating the influence of its neighboring pixels (based 
 * on a MRF model) and finally, classifies each pixel to the class 
 * which has the minimum distance to that pixel (taking the neighborhood 
 * influence under consideration).
 *
 * The a classified initial labeled image is needed. It is important
 * that the number of expected classes be set before calling the 
 * classifier. In our case we have used the GaussianSupervisedClassifer to
 * generate the initial labels. This classifier requires the user to 
 * ensure that an appropriate training image set be provided. See the 
 * documentation of the classifier class for more information.
 *
 * The influence of a neighborhood on a given pixel's
 * classification (the MRF term) is computed by calculating a weighted
 * sum of number of class labels in a three dimensional neighborhood.
 * The basic idea of this neighborhood influence is that if a large
 * number of neighbors of a pixel are of one class, then the current
 * pixel is likely to be of the same class.
 *
 * The dimensions of the neighborhood is same as the input image dimension
 * and values of the weighting 
 * parameters are either specified by the user through the beta matrix
 * parameter. The default weighting table is generated during object 
 * construction. The following table shows an example of a 3x3x3 
 * neighborhood and the weighting values used. A 3 x 3 x 3 kernel
 * is used where each value is a nonnegative parameter, which encourages 
 * neighbors to be of the same class. In this example, the influence of
 * the pixels in the same slice is assigned a weight 1.7, the influence
 * of the pixels in the same location in the previous and next slice is 
 * assigned a weight 1.5, while a weight 1.3 is assigned to the influence of 
 * the north, south, east, west and diagonal pixels in the previous and next 
 * slices. 
 * \f[\begin{tabular}{ccc}
 *  \begin{tabular}{|c|c|c|}
 *   1.3 & 1.3 & 1.3 \\
 *   1.3 & 1.5 & 1.3 \\
 *   1.3 & 1.3 & 1.3 \\
 *  \end{tabular} &
 *  \begin{tabular}{|c|c|c|}
 *   1.7 & 1.7 & 1.7 \\
 *   1.7 & 0 & 1.7 \\
 *   1.7 & 1.7 & 1.7 \\
 *  \end{tabular} &
 *  \begin{tabular}{|c|c|c|}
 *   1.3 & 1.3 & 1.3 \\
 *   1.5 & 1.5 & 1.3 \\
 *   1.3 & 1.3 & 1.3 \\
 *  \end{tabular} \\
 * \end{tabular}\f]
 *
 * The user needs to set the neighborhood size using the SetNeighborhoodRadius
 * functions. The details on the semantics of a neighborhood can be found
 * in the documentation associated with the itkNeighborhood and related 
 * objects. NOTE: The size of the neighborhood must match with the size of 
 * the neighborhood weighting parameters set by the user.
 *
 * For minimization of the MRF labeling function the MinimizeFunctional
 * virtual method is called. For our current implementation we use the
 * the iterated conditional modes (ICM) algorithm described by Besag in the
 * paper ``On the Statistical Analysis of Dirty Pictures'' in J. Royal Stat.
 * Soc. B, Vol. 48, 1986. 
 *
 * In each iteration, the algorithm visits each pixel in turn and 
 * determines whether to update its classification by computing the influence
 * of the classification of the pixel's neighbors and of the intensity data.
 * On each iteration after the first, we reexamine the classification of a 
 * pixel only if the classification of some of its neighbors has changed
 * in the previous iteration. The pixels' classification is updated using a 
 * synchronous scheme (iteration by iteration) until the error reaches
 * less than the threshold or the number of iteration exceed the maximum set
 * number of iterations. 
 *
 * \ingroup MRFFilters
 * \sa Neighborhood \sa ImageIterator \sa NeighborhoodIterator
 * \sa SmartNeighborhoodIterator \sa RandomAccessNeighborhoodIterator
 * \sa Classifier
 */
template <class TInputImage, class TClassifiedImage>
class ITK_EXPORT MRFImageFilter : 
  public ImageToImageFilter<TInputImage,TClassifiedImage>
{
public:       
  /** Standard class typedefs. */
  typedef MRFImageFilter   Self;
  typedef ImageToImageFilter<TInputImage,TClassifiedImage> Superclass;
  typedef SmartPointer<Self>  Pointer;
  typedef SmartPointer<const Self>  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(MRFImageFilter,Object);

  /** Type definition for the input image. */
  typedef typename TInputImage::Pointer              InputImagePointer;  

  /** Type definition for the input image pixel type. */
  typedef typename TInputImage::PixelType            InputImagePixelType;

  /** Type definition for the input image region type. */
  typedef typename TInputImage::RegionType           InputImageRegionType;

  /** Type definition for the input image region iterator */
  typedef ImageRegionIterator<TInputImage>  InputImageRegionIterator;

  /** Image dimension */
  enum{ InputImageDimension = TInputImage::ImageDimension };

  /** Type definitions for the training image. */
  typedef typename TClassifiedImage::Pointer         TrainingImagePointer;

  /** Type definitions for the training image pixel type. */
  typedef typename TClassifiedImage::PixelType       TrainingImagePixelType;

  /** Type definitions for the labelled image.
   * It is derived from the training image. */
  typedef typename TClassifiedImage::Pointer         LabelledImagePointer;
      
  /** Type definitions for the classified image pixel type.
   * It has to be the same type as the training image. */
  typedef typename TClassifiedImage::PixelType       LabelledImagePixelType;

  /** Type definitions for the classified image pixel type.
   * It has to be the same type as the training image. */
  typedef typename TClassifiedImage::RegionType      LabelledImageRegionType;

  /** Type definition for the classified image index type. */
  typedef typename TClassifiedImage::IndexType       LabelledImageIndexType;

  /** Type definition for the classified image offset type. */
  typedef typename TClassifiedImage::OffsetType      LabelledImageOffsetType;

  /** Type definition for the input image region iterator */
  typedef ImageRegionIterator<TClassifiedImage>  
    LabelledImageRegionIterator;

  /** Labelled Image dimension */
  enum{ ClassifiedImageDimension = TClassifiedImage::ImageDimension };

  /** Type definitions for classifier to be used for the MRF lavbelling. */
  typedef Classifier<TInputImage,TClassifiedImage> ClassifierType;

  /** Size and value typedef support. */
  typedef Size<InputImageDimension> SizeType;

  /** Radius typedef support. */
  typedef Size<InputImageDimension> NeighborhoodRadiusType;

  /** Input image neighborhood iterator and kernel size typedef */
  typedef ConstNeighborhoodIterator< TInputImage >
    InputImageNeighborhoodIterator;

  typedef typename InputImageNeighborhoodIterator::RadiusType 
    InputImageNeighborhoodRadiusType;

  typedef NeighborhoodAlgorithm::ImageBoundaryFacesCalculator< TInputImage >
      InputImageFacesCalculator;
  
  typedef typename InputImageFacesCalculator::FaceListType
    InputImageFaceListType;

  typedef typename InputImageFaceListType::iterator 
    InputImageFaceListIterator;

  /** Labelled image neighborhood interator typedef */
  typedef NeighborhoodIterator< TClassifiedImage >
    LabelledImageNeighborhoodIterator;

  typedef typename LabelledImageNeighborhoodIterator::RadiusType 
    LabelledImageNeighborhoodRadiusType;

  typedef NeighborhoodAlgorithm::ImageBoundaryFacesCalculator< TClassifiedImage >
      LabelledImageFacesCalculator;
  
  typedef typename LabelledImageFacesCalculator::FaceListType
    LabelledImageFaceListType;

  typedef typename LabelledImageFaceListType::iterator 
    LabelledImageFaceListIterator;

  /** Set the image required for training type classifiers. */
  void SetTrainingImage(TrainingImagePointer image);

  /** Get the traning image.  */
  TrainingImagePointer GetTrainingImage();

  /** Set the pointer to the classifer being used. */
  void SetClassifier( typename ClassifierType::Pointer ptrToClassifier );

  /** Set/Get the number of classes. */
  itkSetMacro(NumberOfClasses, unsigned int);
  itkGetMacro(NumberOfClasses, unsigned int);

  /** Set/Get the number of iteration of the Iterated Conditional Mode
   * (ICM) algorithm. A default value is set at 50 iterations. */
  itkSetMacro(MaximumNumberOfIterations, unsigned int);
  itkGetMacro(MaximumNumberOfIterations, unsigned int);

  /** Set/Get the error tollerance level which is used as a threshold
   * to quit the iterations */
  itkSetMacro(ErrorTolerance, double);
  itkGetMacro(ErrorTolerance, double);

  /** Set the neighborhood radius */
  void SetNeighborhoodRadius(const SizeType &);  

  /** Sets the radius for the neighborhood, calculates size from the
   * radius, and allocates storage. */

  void SetNeighborhoodRadius( const unsigned long );
  void SetNeighborhoodRadius( const unsigned long *radiusArray );  

  /** Get the neighborhood radius */
  const NeighborhoodRadiusType GetNeighborhoodRadius() const
    { 
      NeighborhoodRadiusType radius;
      
      for(int i=0; i<InputImageDimension; ++i)
        radius[i] = m_InputImageNeighborhoodRadius[i];

      return radius;
    }
    

  /** Set the weighting parameters (used in MRF algorithms). This is a
   * function allowing the users to set the weight matrix by providing a 
   * a 1D array of weights. The default implementation supports  a 
   * 3 x 3 x 3 kernel. The labeler needs to be extended for a different
   * kernel size. */
  virtual void SetMRFNeighborhoodWeight( std::vector<double> BetaMatrix );
  virtual std::vector<double> GetMRFNeighborhoodWeight()
    {
    return m_MRFNeighborhoodWeight;
    }
      
protected:
  MRFImageFilter();
  ~MRFImageFilter();
  void PrintSelf(std::ostream& os, Indent indent) const;

  /** Allocate memory for labelled images. */
  void Allocate();

  /** Apply MRF Classifier. In this example the images are labelled using
   * Iterated Conditional Mode algorithm by J. Besag, "On statistical
   * analysis of dirty pictures," J. Royal Stat. Soc. B, vol. 48,
   * pp. 259-302, 1986. */
  virtual void ApplyMRFImageFilter();  

  /** Minimization algorithm to be used. */
  virtual void MinimizeFunctional();

  virtual void GenerateData();
  virtual void GenerateInputRequestedRegion();
  virtual void EnlargeOutputRequestedRegion( DataObject * );
  virtual void GenerateOutputInformation();

private:            
  MRFImageFilter(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented
  
  typedef typename TInputImage::SizeType InputImageSizeType;


  typedef Image<int,InputImageDimension > LabelStatusImageType;
  typedef typename LabelStatusImageType::IndexType LabelStatusIndexType;
  typedef typename LabelStatusImageType::RegionType LabelStatusRegionType;
  typedef typename LabelStatusImageType::Pointer LabelStatusImagePointer;
  typedef ImageRegionIterator< LabelStatusImageType > 
    LabelStatusImageIterator;

  /** Labelled status image neighborhood interator typedef */
  typedef NeighborhoodIterator< LabelStatusImageType >
    LabelStatusImageNeighborhoodIterator;

  typedef typename LabelStatusImageNeighborhoodIterator::RadiusType 
    LabelStatusImageNeighborhoodRadiusType;

  typedef NeighborhoodAlgorithm::ImageBoundaryFacesCalculator< LabelStatusImageType >
      LabelStatusImageFacesCalculator;
  
  typedef typename LabelStatusImageFacesCalculator::FaceListType
    LabelStatusImageFaceListType;

  typedef typename LabelStatusImageFaceListType::iterator 
    LabelStatusImageFaceListIterator;

  InputImageNeighborhoodRadiusType       m_InputImageNeighborhoodRadius;
  LabelledImageNeighborhoodRadiusType    m_LabelledImageNeighborhoodRadius;
  LabelStatusImageNeighborhoodRadiusType m_LabelStatusImageNeighborhoodRadius;
 
  unsigned int              m_NumberOfClasses;
  unsigned int              m_MaximumNumberOfIterations;
  unsigned int              m_KernelSize;
  int                       m_ErrorCounter;
  int                       m_NeighborhoodSize;
  int                       m_TotalNumberOfValidPixelsInOutputImage;
  int                       m_TotalNumberOfPixelsInInputImage;
  double                    m_ErrorTolerance;
  double                    *m_ClassProbability; //Class liklihood

  LabelStatusImagePointer   m_LabelStatusImage;

  std::vector<double>       m_MRFNeighborhoodWeight;
  std::vector<double>       m_NeighborInfluence;
  std::vector<double>       m_MahalanobisDistance;

  /** Pointer to the classifier to be used for the MRF labelling. */
  typename ClassifierType::Pointer m_ClassifierPtr;


  /** Set/Get the weighting parameters (Beta Matrix). A default 3 x 3 x 3 
   * matrix is provided. However, the user is allowed to override it
   * with their choice of weights for a 3 x 3 x 3 matrix. */
  virtual void SetMRFNeighborhoodWeight( double* );

  //Function implementing the ICM algorithm to label the images
  void ApplyICMLabeller();

  //Function implementing the neighborhood operation
  void DoNeighborhoodOperation( const InputImageNeighborhoodIterator &imageIter,
         LabelledImageNeighborhoodIterator &labelledIter,
         LabelStatusImageNeighborhoodIterator &labelStatusIter );


}; // class MRFImageFilter


} // namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#include "itkMRFImageFilter.txx"
#endif



#endif


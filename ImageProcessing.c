 
//**************************************************************************
//* WARNING: This file was automatically generated.  Any changes you make  *
//*          to this file will be lost if you generate the file again.     *
//**************************************************************************
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <nivision.h>
#include <nimachinevision.h>
#include <windows.h>

// If you call Machine Vision functions in your script, add NIMachineVision.c to the project.

#define IVA_MAX_BUFFERS 20
#define IVA_STORE_RESULT_NAMES

#define VisionErrChk(Function) {if (!(Function)) {success = 0; goto Error;}}

typedef enum IVA_ResultType_Enum {IVA_NUMERIC, IVA_BOOLEAN, IVA_STRING} IVA_ResultType;

typedef union IVA_ResultValue_Struct    // A result in Vision Assistant can be of type double, BOOL or string.
{
    double numVal;
    BOOL   boolVal;
    char*  strVal;
} IVA_ResultValue;

typedef struct IVA_Result_Struct
{
#if defined (IVA_STORE_RESULT_NAMES)
    char resultName[256];           // Result name
#endif
    IVA_ResultType  type;           // Result type
    IVA_ResultValue resultVal;      // Result value
} IVA_Result;

typedef struct IVA_StepResultsStruct
{
#if defined (IVA_STORE_RESULT_NAMES)
    char stepName[256];             // Step name
#endif
    int         numResults;         // number of results created by the step
    IVA_Result* results;            // array of results
} IVA_StepResults;

typedef struct IVA_Data_Struct
{
    Image* buffers[IVA_MAX_BUFFERS];            // Vision Assistant Image Buffers
    IVA_StepResults* stepResults;              // Array of step results
    int numSteps;                               // Number of steps allocated in the stepResults array
    CoordinateSystem *baseCoordinateSystems;    // Base Coordinate Systems
    CoordinateSystem *MeasurementSystems;       // Measurement Coordinate Systems
    int numCoordSys;                            // Number of coordinate systems
} IVA_Data;



static IVA_Data* IVA_InitData(int numSteps, int numCoordSys);
static int IVA_DisposeData(IVA_Data* ivaData);
static int IVA_DisposeStepResults(IVA_Data* ivaData, int stepIndex);
static int IVA_CLRThreshold(Image* image, int min1, int max1, int min2, int max2, int min3, int max3, int colorMode);
static int IVA_DetectLines(Image* image,
                                    IVA_Data* ivaData,
                                    double minLength,
                                    double maxLength,
                                    int extraction,
                                    int curveThreshold,
                                    int edgeFilterSize,
                                    int curveMinLength,
                                    int curveRowStepSize,
                                    int curveColumnStepSize,
                                    int curveMaxEndPointGap,
                                    int matchMode, 
                                    float rangeMin[],
                                    float rangeMax[],
                                    float score,
                                    ROI* roi,
                                    int stepIndex);

int IVA_ProcessImage(Image *image)
{
	int success = 1;
    IVA_Data *ivaData;
    ImageType imageType;
    ROI *roi;
    float rangeMin[3] = {0,0,50};
    float rangeMax[3] = {360,0,200};

    // Initializes internal data (buffers and array of points for caliper measurements)
    VisionErrChk(ivaData = IVA_InitData(4, 0));

	VisionErrChk(IVA_CLRThreshold(image, 0, 255, 71, 255, 184, 255, IMAQ_HSL));

    //-------------------------------------------------------------------//
    //                       Lookup Table: Equalize                      //
    //-------------------------------------------------------------------//
    // Calculates the histogram of the image and redistributes pixel values across
    // the desired range to maintain the same pixel value distribution.
    VisionErrChk(imaqGetImageType(image, &imageType));
    VisionErrChk(imaqEqualize(image, image, 0, (imageType == IMAQ_IMAGE_U8 ? 255 : 0), NULL));

    //-------------------------------------------------------------------//
    //                       Lookup Table: Inverse                       //
    //-------------------------------------------------------------------//

    // Inverts the pixel intensities of the image.
    VisionErrChk(imaqInverse(image, image, NULL));

    // Creates a new, empty region of interest.
    VisionErrChk(roi = imaqCreateROI());

    // Creates a new rectangle ROI contour and adds the rectangle to the provided ROI.
    VisionErrChk(imaqAddRectContour(roi, imaqMakeRect(20, 20, 320, 600)));

    //-------------------------------------------------------------------//
    //                            Detect Lines                           //
    //-------------------------------------------------------------------//

	VisionErrChk(IVA_DetectLines(image, ivaData, 50, 500, IMAQ_NORMAL_IMAGE, 
		75, IMAQ_NORMAL, 25, 15, 15, 10, 1, rangeMin, rangeMax, 800, roi, 3));

    // Cleans up resources associated with the object
    imaqDispose(roi);

    // Releases the memory allocated in the IVA_Data structure.
    IVA_DisposeData(ivaData);

Error:
	return success;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function Name: IVA_CLRThreshold
//
// Description  : Thresholds a color image.
//
// Parameters   : image      -  Input image
//                min1       -  Minimum range for the first plane
//                max1       -  Maximum range for the first plane
//                min2       -  Minimum range for the second plane
//                max2       -  Maximum range for the second plane
//                min3       -  Minimum range for the third plane
//                max3       -  Maximum range for the third plane
//                colorMode  -  Color space in which to perform the threshold
//
// Return Value : success
//
////////////////////////////////////////////////////////////////////////////////
static int IVA_CLRThreshold(Image* image, int min1, int max1, int min2, int max2, int min3, int max3, int colorMode)
{
    int success = 1;
    Image* thresholdImage;
    Range plane1Range;
    Range plane2Range;
    Range plane3Range;


    //-------------------------------------------------------------------//
    //                          Color Threshold                          //
    //-------------------------------------------------------------------//

    // Creates an 8 bit image for the thresholded image.
    VisionErrChk(thresholdImage = imaqCreateImage(IMAQ_IMAGE_U8, 7));

    // Set the threshold range for the 3 planes.
    plane1Range.minValue = min1;
    plane1Range.maxValue = max1;
    plane2Range.minValue = min2;
    plane2Range.maxValue = max2;
    plane3Range.minValue = min3;
    plane3Range.maxValue = max3;

    // Thresholds the color image.
    VisionErrChk(imaqColorThreshold(thresholdImage, image, 1, colorMode, &plane1Range, &plane2Range, &plane3Range));

    // Copies the threshold image in the souce image.
    VisionErrChk(imaqDuplicate(image, thresholdImage));

Error:
    imaqDispose(thresholdImage);

    return success;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function Name: IVA_DetectLines
//
// Description  : Searches for lines in an image that are within a given range
//
// Parameters   : image                -  Input image
//                ivaData              -  Internal Data structure
//                minLength            -  Minimum Length
//                maxLength            -  Maximum Length 
//                extraction           -  Extraction mode
//                curveThreshold       -  Specifies the minimum contrast at a
//                                        pixel for it to be considered as part
//                                        of a curve.
//                edgeFilterSize       -  Specifies the width of the edge filter
//                                        the function uses to identify curves in
//                                        the image.
//                curveMinLength       -  Specifies the smallest curve the
//                                        function will identify as a curve.
//                curveRowStepSize     -  Specifies the distance, in the x direction
//                                        between two pixels the function inspects
//                                        for curve seed points.
//                curveColumnStepSize  -  Specifies the distance, in the y direction,
//                                        between two pixels the function inspects
//                                        for curve seed points.
//                curveMaxEndPointGap  -  Specifies the maximum gap, in pixels,
//                                        between the endpoints of a curve that the
//                                        function identifies as a closed curve.
//                matchMode            -  Specifies the method to use when looking
//                                        for the pattern in the image.
//                rangeMin             -  Match constraints range min array
//                                        (angle 1, angle 2, scale, occlusion)
//                rangeMax             -  Match constraints range max array
//                                        (angle 1, angle 2, scale, occlusion)
//                score                -  Minimum score a match can have for the
//                                        function to consider the match valid.
//                roi                  -  Search area
//                stepIndex            -  Step index (index at which to store the results in the resuts array)
//
// Return Value : success
//
////////////////////////////////////////////////////////////////////////////////
static int IVA_DetectLines(Image* image,
                                    IVA_Data* ivaData,
                                    double minLength,
                                    double maxLength,
                                    int extraction,
                                    int curveThreshold,
                                    int edgeFilterSize,
                                    int curveMinLength,
                                    int curveRowStepSize,
                                    int curveColumnStepSize,
                                    int curveMaxEndPointGap,
                                    int matchMode, 
                                    float rangeMin[],
                                    float rangeMax[],
                                    float score,
                                    ROI* roi,
                                    int stepIndex)
{
    int success = 1;
    LineDescriptor lineDescriptor;     
    CurveOptions curveOptions;
    ShapeDetectionOptions shapeOptions; 
    RangeFloat orientationRange[2]; 
    int i;
    LineMatch* matchedLines = NULL; 
    int numMatchesFound;
    int numObjectResults;
    IVA_Result* shapeMacthingResults;
    unsigned int visionInfo;
    PointFloat linePoints[2];
    TransformReport* realWorldPosition = NULL;
    float calibratedLength;


    //-------------------------------------------------------------------//
    //                        Detect Lines                               //
    //-------------------------------------------------------------------//

    // Fill in the Curve options.
    curveOptions.extractionMode = extraction;
    curveOptions.threshold = curveThreshold;
    curveOptions.filterSize = edgeFilterSize;
    curveOptions.minLength = curveMinLength;
    curveOptions.rowStepSize = curveRowStepSize;
    curveOptions.columnStepSize = curveColumnStepSize;
    curveOptions.maxEndPointGap = curveMaxEndPointGap;
    curveOptions.onlyClosed = 0;
    curveOptions.subpixelAccuracy = 0;

    lineDescriptor.minLength = minLength;
    lineDescriptor.maxLength = maxLength;

    for (i = 0 ; i < 2 ; i++)
    {
        orientationRange[i].minValue = rangeMin[i];
        orientationRange[i].maxValue = rangeMax[i];
    }

    shapeOptions.mode = matchMode;
    shapeOptions.angleRanges = orientationRange;
    shapeOptions.numAngleRanges = 2;
    shapeOptions.scaleRange.minValue = rangeMin[2];
    shapeOptions.scaleRange.maxValue = rangeMax[2];
    shapeOptions.minMatchScore = score;

    matchedLines = NULL;
    numMatchesFound = 0;

    // Searches for lines in the image that are within the range.
    VisionErrChk(matchedLines = imaqDetectLines(image, &lineDescriptor, &curveOptions, &shapeOptions, roi, &numMatchesFound));

    // ////////////////////////////////////////
    // Store the results in the data structure.
    // ////////////////////////////////////////
    
    // First, delete all the results of this step (from a previous iteration)
    IVA_DisposeStepResults(ivaData, stepIndex);

    // Check if the image is calibrated.
    VisionErrChk(imaqGetVisionInfoTypes(image, &visionInfo));

    // If the image is calibrated, we also need to log the calibrated position (x and y) -> 12 results instead of 7
    numObjectResults = (visionInfo & IMAQ_VISIONINFO_CALIBRATION ? 12 : 7);

    ivaData->stepResults[stepIndex].numResults = numMatchesFound * numObjectResults + 1;
    ivaData->stepResults[stepIndex].results = malloc (sizeof(IVA_Result) * ivaData->stepResults[stepIndex].numResults);
    shapeMacthingResults = ivaData->stepResults[stepIndex].results;
    
    if (shapeMacthingResults)
    {
        #if defined (IVA_STORE_RESULT_NAMES)
            sprintf(shapeMacthingResults->resultName, "# Matches");
        #endif
        shapeMacthingResults->type = IVA_NUMERIC;
        shapeMacthingResults->resultVal.numVal = numMatchesFound;
        shapeMacthingResults++;
        
        for (i = 0 ; i < numMatchesFound ; i++)
        {
            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.Score", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].score;
            shapeMacthingResults++;
            
            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.Length (Pix.)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].length;
            shapeMacthingResults++;
            
            if (visionInfo & IMAQ_VISIONINFO_CALIBRATION)
            {
                linePoints[0] = matchedLines[i].startPoint;
                linePoints[1] = matchedLines[i].endPoint;

                realWorldPosition = imaqTransformPixelToRealWorld(image, linePoints, 2);
                imaqGetDistance (realWorldPosition->points[0], realWorldPosition->points[1], &calibratedLength);
                
                #if defined (IVA_STORE_RESULT_NAMES)
                    sprintf(shapeMacthingResults->resultName, "Line %d.Length (World)", i + 1);
                #endif
                shapeMacthingResults->type = IVA_NUMERIC;
                shapeMacthingResults->resultVal.numVal = calibratedLength;
                shapeMacthingResults++;
            }

            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.Angle (degrees)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].rotation;
            shapeMacthingResults++;

            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.Start Point X (Pix.)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].startPoint.x;
            shapeMacthingResults++;

            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.Start Point Y (Pix.)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].startPoint.y;
            shapeMacthingResults++;
            
            if (visionInfo & IMAQ_VISIONINFO_CALIBRATION)
            {
                #if defined (IVA_STORE_RESULT_NAMES)
                    sprintf(shapeMacthingResults->resultName, "Line %d.Start Point X (World)", i + 1);
                #endif
                shapeMacthingResults->type = IVA_NUMERIC;
                shapeMacthingResults->resultVal.numVal = realWorldPosition->points[0].x;
                shapeMacthingResults++;
                
                #if defined (IVA_STORE_RESULT_NAMES)
                    sprintf(shapeMacthingResults->resultName, "Line %d.Start Point Y (World)", i + 1);
                #endif
                shapeMacthingResults->type = IVA_NUMERIC;
                shapeMacthingResults->resultVal.numVal = realWorldPosition->points[0].y;
                shapeMacthingResults++;
            }
            
            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.End Point X (Pix.)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].endPoint.x;
            shapeMacthingResults++;

            #if defined (IVA_STORE_RESULT_NAMES)
                sprintf(shapeMacthingResults->resultName, "Line %d.End Point Y (Pix.)", i + 1);
            #endif
            shapeMacthingResults->type = IVA_NUMERIC;
            shapeMacthingResults->resultVal.numVal = matchedLines[i].endPoint.y;
            shapeMacthingResults++;
            
            if (visionInfo & IMAQ_VISIONINFO_CALIBRATION)
            {
                #if defined (IVA_STORE_RESULT_NAMES)
                    sprintf(shapeMacthingResults->resultName, "Line %d.End Point X (World)", i + 1);
                #endif
                shapeMacthingResults->type = IVA_NUMERIC;
                shapeMacthingResults->resultVal.numVal = realWorldPosition->points[1].x;
                shapeMacthingResults++;
                
                #if defined (IVA_STORE_RESULT_NAMES)
                    sprintf(shapeMacthingResults->resultName, "Line %d.End Point Y (World)", i + 1);
                #endif
                shapeMacthingResults->type = IVA_NUMERIC;
                shapeMacthingResults->resultVal.numVal = realWorldPosition->points[1].y;
                shapeMacthingResults++;
            }
        }
    }

Error:
    // Disposes temporary image and structures.
    imaqDispose(matchedLines);
    imaqDispose(realWorldPosition);

    return success;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function Name: IVA_InitData
//
// Description  : Initializes data for buffer management and results.
//
// Parameters   : # of steps
//                # of coordinate systems
//
// Return Value : success
//
////////////////////////////////////////////////////////////////////////////////
static IVA_Data* IVA_InitData(int numSteps, int numCoordSys)
{
    int success = 1;
    IVA_Data* ivaData = NULL;
    int i;


    // Allocate the data structure.
    VisionErrChk(ivaData = (IVA_Data*)malloc(sizeof (IVA_Data)));

    // Initializes the image pointers to NULL.
    for (i = 0 ; i < IVA_MAX_BUFFERS ; i++)
        ivaData->buffers[i] = NULL;

    // Initializes the steo results array to numSteps elements.
    ivaData->numSteps = numSteps;

    ivaData->stepResults = (IVA_StepResults*)malloc(ivaData->numSteps * sizeof(IVA_StepResults));
    for (i = 0 ; i < numSteps ; i++)
    {
        #if defined (IVA_STORE_RESULT_NAMES)
            sprintf(ivaData->stepResults[i].stepName, "");
        #endif
        ivaData->stepResults[i].numResults = 0;
        ivaData->stepResults[i].results = NULL;
    }

    // Create the coordinate systems
	ivaData->baseCoordinateSystems = NULL;
	ivaData->MeasurementSystems = NULL;
	if (numCoordSys)
	{
		ivaData->baseCoordinateSystems = (CoordinateSystem*)malloc(sizeof(CoordinateSystem) * numCoordSys);
		ivaData->MeasurementSystems = (CoordinateSystem*)malloc(sizeof(CoordinateSystem) * numCoordSys);
	}

    ivaData->numCoordSys = numCoordSys;

Error:
    return ivaData;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function Name: IVA_DisposeData
//
// Description  : Releases the memory allocated in the IVA_Data structure
//
// Parameters   : ivaData  -  Internal data structure
//
// Return Value : success
//
////////////////////////////////////////////////////////////////////////////////
static int IVA_DisposeData(IVA_Data* ivaData)
{
    int i;


    // Releases the memory allocated for the image buffers.
    for (i = 0 ; i < IVA_MAX_BUFFERS ; i++)
        imaqDispose(ivaData->buffers[i]);

    // Releases the memory allocated for the array of measurements.
    for (i = 0 ; i < ivaData->numSteps ; i++)
        IVA_DisposeStepResults(ivaData, i);

    free(ivaData->stepResults);

    // Dispose of coordinate systems
    if (ivaData->numCoordSys)
    {
        free(ivaData->baseCoordinateSystems);
        free(ivaData->MeasurementSystems);
    }

    free(ivaData);

    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function Name: IVA_DisposeStepResults
//
// Description  : Dispose of the results of a specific step.
//
// Parameters   : ivaData    -  Internal data structure
//                stepIndex  -  step index
//
// Return Value : success
//
////////////////////////////////////////////////////////////////////////////////
static int IVA_DisposeStepResults(IVA_Data* ivaData, int stepIndex)
{
    int i;

    
    for (i = 0 ; i < ivaData->stepResults[stepIndex].numResults ; i++)
    {
        if (ivaData->stepResults[stepIndex].results[i].type == IVA_STRING)
            free(ivaData->stepResults[stepIndex].results[i].resultVal.strVal);
    }

    free(ivaData->stepResults[stepIndex].results);

    return TRUE;
}



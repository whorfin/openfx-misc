/*
 OFX TestRender plugin.

 Copyright (C) 2014 INRIA

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France

 */

#include "TestRender.h"

#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "ofxsProcessing.H"
#include "ofxsMaskMix.h"


#define kPluginName "TestRenderOFX"
#define kPluginGrouping "Other/Test"
#define kPluginDescription \
"Test rendering by the host.\n" \
"This plugin paints pixel dots on the upper left and lower right parts of the input image. " \
"The dots are spaced by 1 pixel at each render scale. " \
"White dots are painted at coordinates which are multiples of 10. " \
"Color dots are painted are coordinates which are multiples of 2, " \
"and the color depends on the render scale " \
"(respectively cyan, magenta, yellow, red, green, blue for levels 0, 1, 2, 3, 4, 5)." \
"Several versions of this plugin are available, with support for tiling enabled/disabled (TiOK/TiNo), " \
"multiresolution enabled/disabled (MrOK/MrNo), render scale support enabled/disabled (RsOK/RsNo)." \
"The effect returns a region-dependent value for isIdentity."

#define kPluginIdentifier "net.sf.openfx:TestRender"
#define kPluginVersionMajor 1 // Incrementing this number means that you have broken backwards compatibility of the plug-in.
#define kPluginVersionMinor 0 // Increment this when you have fixed a bug or made it faster.

#define kParamColor0      "color0"
#define kParamColor0Label "Color 0"
#define kParamColor0Hint  "Color for render scale level 0"
#define kParamColor1      "color1"
#define kParamColor1Label "Color 1"
#define kParamColor1Hint  "Color for render scale level 1"
#define kParamColor2      "color2"
#define kParamColor2Label "Color 2"
#define kParamColor2Hint  "Color for render scale level 2"
#define kParamColor3      "color3"
#define kParamColor3Label "Color 3"
#define kParamColor3Hint  "Color for render scale level 3"
#define kParamColor4      "color4"
#define kParamColor4Label "Color 4"
#define kParamColor4Hint  "Color for render scale level 4"
#define kParamColor5      "color5"
#define kParamColor5Label "Color 5"
#define kParamColor5Hint  "Color for render scale level 5"

#define kParamClipInfo      "clipInfo"
#define kParamClipInfoLabel "Clip Info..."
#define kParamClipInfoHint  "Display information about the inputs"

#define kParamIdentityEven "identityEven"
#define kParamIdentityEvenLabel "Identity for even levels"
#define kParamIdentityEvenHint "Even levels of the render scale (including full resolution) return the input image (isIdentity is true for these levels)"

#define kParamIdentityOdd "identityOdd"
#define kParamIdentityOddLabel "Identity for odd levels"
#define kParamIdentityOddHint "Odd levels of the render scale return the input image (isIdentity is true for these levels"

#define kParamForceCopy "forceCopy"
#define kParamForceCopyLabel "Force Copy"
#define kParamForceCopyHint "Force copy from input to output (isIdentity always returns false)"

using namespace OFX;

// Base class for the RGBA and the Alpha processor
class TestRenderBase : public OFX::ImageProcessor
{
protected:
    const OFX::Image *_srcImg;
    const OFX::Image *_maskImg;
    bool   _doMasking;

    double _mix;
    bool _maskInvert;

public:
    /** @brief no arg ctor */
    TestRenderBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImg(0)
    , _maskImg(0)
    , _doMasking(false)
    , _mix(1.)
    , _maskInvert(false)
    {
    }

    /** @brief set the src image */
    void setSrcImg(const OFX::Image *v) {_srcImg = v;}

    void setMaskImg(const OFX::Image *v) {_maskImg = v;}

    void doMasking(bool v) {_doMasking = v;}

    void setValues(double mix, bool maskInvert)
    {
        _mix = mix;
        _maskInvert = maskInvert;
    }
};

// template to do the RGBA processing
template <class PIX, int nComponents, int maxValue>
class ImageTestRenderer : public TestRenderBase
{
public:
    // ctor
    ImageTestRenderer(OFX::ImageEffect &instance)
    : TestRenderBase(instance)
    {
    }

private:
    // and do some processing
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        float tmpPix[nComponents];
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if (_effect.abort()) {
                break;
            }

            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

            for (int x = procWindow.x1; x < procWindow.x2; x++) {

                const PIX *srcPix = (const PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);

                // do we have a source image to scale up
                if (srcPix) {
                    for (int c = 0; c < nComponents; ++c) {
                        tmpPix[c] = (maxValue - srcPix[c]);
                    }
                } else {
                    // no src pixel here, be black and transparent
                    for (int c = 0; c < nComponents; ++c) {
                        tmpPix[c] = (maxValue - 0.);
                    }
                }
                ofxsMaskMixPix<PIX, nComponents, maxValue, true>(tmpPix, x, y, srcPix, _doMasking, _maskImg, _mix, _maskInvert, dstPix);

                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
class TestRenderPlugin : public OFX::ImageEffect
{
public:
    /** @brief ctor */
    TestRenderPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , dstClip_(0)
    , srcClip_(0)
    {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        assert(dstClip_ && (dstClip_->getPixelComponents() == ePixelComponentRGB || dstClip_->getPixelComponents() == ePixelComponentRGBA || dstClip_->getPixelComponents() == ePixelComponentAlpha));
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(srcClip_ && (srcClip_->getPixelComponents() == ePixelComponentRGB || srcClip_->getPixelComponents() == ePixelComponentRGBA || srcClip_->getPixelComponents() == ePixelComponentAlpha));
        maskClip_ = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!maskClip_ || maskClip_->getPixelComponents() == ePixelComponentAlpha);

        _color[0] = fetchRGBAParam(kParamColor0);
        _color[1] = fetchRGBAParam(kParamColor0);
        _color[2] = fetchRGBAParam(kParamColor0);
        _color[3] = fetchRGBAParam(kParamColor0);
        _color[4] = fetchRGBAParam(kParamColor0);
        _color[5] = fetchRGBAParam(kParamColor0);
        assert(_color[0] && _color[1] && _color[2] && _color[3] && _color[4] && _color[5]);

        _identityEven = fetchBooleanParam(kParamIdentityEven);
        _identityOdd = fetchBooleanParam(kParamIdentityOdd);
        _forceCopy = fetchBooleanParam(kParamForceCopy);
        assert(_identityEven && _identityOdd && _forceCopy);

        _mix = fetchDoubleParam(kMixParamName);
        _maskInvert = fetchBooleanParam(kMaskInvertParamName);
        assert(_mix && _maskInvert);
    }

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) /*OVERRIDE FINAL*/;

    /* set up and run a processor */
    void setupAndProcess(TestRenderBase &, const OFX::RenderArguments &args);

    virtual bool isIdentity(const RenderArguments &args, Clip * &identityClip, double &identityTime) /*OVERRIDE FINAL*/;

    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName);

    // override the rod call
    virtual bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod);

private:
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *dstClip_;
    OFX::Clip *srcClip_;
    OFX::Clip *maskClip_;

    OFX::RGBAParam* _color[6];
    OFX::BooleanParam* _identityEven;
    OFX::BooleanParam* _identityOdd;
    OFX::BooleanParam* _forceCopy;

    OFX::DoubleParam* _mix;
    OFX::BooleanParam* _maskInvert;
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from


/* set up and run a processor */
template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
void
TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>::setupAndProcess(TestRenderBase &processor, const OFX::RenderArguments &args)
{
    // get a dst image
    std::auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    if (!dst.get()) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();

    // fetch main input image
    std::auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));

    // make sure bit depths are sane
    if (src.get()) {
        OFX::BitDepthEnum    srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();

        // see if they have the same depths and bytes and all
        if (srcBitDepth != dstBitDepth || srcComponents != dstComponents) {
            OFX::throwSuiteStatusException(kOfxStatErrImageFormat);
        }
        OfxRectI srcRod = src->getRegionOfDefinition();
        OfxRectI srcBounds = src->getBounds();
        OfxRectI dstRod = dst->getRegionOfDefinition();
        OfxRectI dstBounds = dst->getBounds();

        if (!supportsTiles) {
            // http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxImageEffectPropSupportsTiles
            //  If a clip or plugin does not support tiled images, then the host should supply full RoD images to the effect whenever it fetches one.
            assert(srcRod.x1 == srcBounds.x1);
            assert(srcRod.x2 == srcBounds.x2);
            assert(srcRod.y1 == srcBounds.y1);
            assert(srcRod.y2 == srcBounds.y2); // crashes on Natron if kSupportsTiles=0 & kSupportsMultiResolution=1
            assert(dstRod.x1 == dstBounds.x1);
            assert(dstRod.x2 == dstBounds.x2);
            assert(dstRod.y1 == dstBounds.y1);
            assert(dstRod.y2 == dstBounds.y2); // crashes on Natron if kSupportsTiles=0 & kSupportsMultiResolution=1
        }
        if (!supportsMultiResolution) {
            // http://openfx.sourceforge.net/Documentation/1.3/ofxProgrammingReference.html#kOfxImageEffectPropSupportsMultiResolution
            //   Multiple resolution images mean...
            //    input and output images can be of any size
            //    input and output images can be offset from the origin
            assert(srcRod.x1 == 0);
            assert(srcRod.y1 == 0);
            assert(srcRod.x1 == dstRod.x1);
            assert(srcRod.x2 == dstRod.x2);
            assert(srcRod.y1 == dstRod.y1);
            assert(srcRod.y2 == dstRod.y2); // crashes on Natron if kSupportsMultiResolution=0
        }
    }

    // auto ptr for the mask.
    std::auto_ptr<OFX::Image> mask((getContext() != OFX::eContextFilter) ? maskClip_->fetchImage(args.time) : 0);

    // do we do masking
    if (getContext() != OFX::eContextFilter && maskClip_->isConnected()) {
        // say we are masking
        processor.doMasking(true);

        // Set it in the processor
        processor.setMaskImg(mask.get());
    }

    double mix;
    _mix->getValueAtTime(args.time, mix);
    bool maskInvert;
    _maskInvert->getValueAtTime(args.time, maskInvert);
    processor.setValues(mix, maskInvert);

    // set the images
    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());

    // set the render window
    processor.setRenderWindow(args.renderWindow);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

// the overridden render function
template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
void
TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>::render(const OFX::RenderArguments &args)
{
    if (!supportsRenderScale && (args.renderScale.x != 1. || args.renderScale.y != 1.)) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();

    // do the rendering
    if (dstComponents == OFX::ePixelComponentRGBA) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte : {
                ImageTestRenderer<unsigned char, 4, 255> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthUShort : {
                ImageTestRenderer<unsigned short, 4, 65535> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthFloat : {
                ImageTestRenderer<float, 4, 1> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else if (dstComponents == OFX::ePixelComponentRGB) {
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte : {
                ImageTestRenderer<unsigned char, 3, 255> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthUShort : {
                ImageTestRenderer<unsigned short, 3, 65535> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthFloat : {
                ImageTestRenderer<float, 3, 1> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } else {
        assert(dstComponents == OFX::ePixelComponentAlpha);
        switch (dstBitDepth) {
            case OFX::eBitDepthUByte : {
                ImageTestRenderer<unsigned char, 1, 255> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthUShort : {
                ImageTestRenderer<unsigned short, 1, 65535> fred(*this);
                setupAndProcess(fred, args);
            }
                break;

            case OFX::eBitDepthFloat : {
                ImageTestRenderer<float, 1, 1> fred(*this);
                setupAndProcess(fred, args);
            }
                break;
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
}

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
bool
TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>::isIdentity(const RenderArguments &args, Clip * &identityClip, double &identityTime)
{
    if (!supportsRenderScale && (args.renderScale.x != 1. || args.renderScale.y != 1.)) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

   double mix;
    _mix->getValueAtTime(args.time, mix);

    if (mix == 0.) {
        identityClip = srcClip_;
        return true;
    } else {
        return false;
    }
}


static const char*
bitDepthString(BitDepthEnum bitDepth)
{
    switch (bitDepth) {
        case OFX::eBitDepthUByte:
            return "8u";
        case OFX::eBitDepthUShort:
            return "16u";
        case OFX::eBitDepthHalf:
            return "16f";
        case OFX::eBitDepthFloat:
            return "32f";
        case OFX::eBitDepthCustom:
            return "x";
        case OFX::eBitDepthNone:
            return "0";
#ifdef OFX_EXTENSIONS_VEGAS
        case eBitDepthUByteBGRA:
            return "8uBGRA";
        case eBitDepthUShortBGRA:
            return "16uBGRA";
        case eBitDepthFloatBGRA:
            return "32fBGRA";
#endif
        default:
            return "[unknown bit depth]";
    }
}

static std::string
pixelComponentString(const std::string& p)
{
    const std::string prefix = "OfxImageComponent";
    std::string s = p;
    return s.replace(s.find(prefix),prefix.length(),"");
}

static const char*
premultString(PreMultiplicationEnum e)
{
    switch (e) {
        case eImageOpaque:
            return "Opaque";
        case eImagePreMultiplied:
            return "PreMultiplied";
        case eImageUnPreMultiplied:
            return "UnPreMultiplied";
        default:
            return "[unknown premult]";
    }
}

#ifdef OFX_EXTENSIONS_VEGAS
static const char*
pixelOrderString(PixelOrderEnum e)
{
    switch (e) {
        case ePixelOrderRGBA:
            return "RGBA";
        case ePixelOrderBGRA:
            return "BGRA";
        default:
            return "[unknown pixel order]";
    }
}
#endif

static const char*
fieldOrderString(FieldEnum e)
{
    switch (e) {
        case eFieldNone:
            return "None";
        case eFieldBoth:
            return "Both";
        case eFieldLower:
            return "Lower";
        case eFieldUpper:
            return "Upper";
        case eFieldSingle:
            return "Single";
        case eFieldDoubled:
            return "Doubled";
        default:
            return "[unknown field order]";
    }
}

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
void
TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName)
{
    if (!supportsRenderScale && (args.renderScale.x != 1. || args.renderScale.y != 1.)) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    if (paramName == kParamClipInfo) {
        std::ostringstream oss;
        oss << "Clip Info:\n\n";
        oss << "Input: ";
        if (!srcClip_) {
            oss << "N/A";
        } else {
            OFX::Clip &c = *srcClip_;
            oss << pixelComponentString(c.getPixelComponentsProperty());
            oss << bitDepthString(c.getPixelDepth());
            oss << " (unmapped: ";
            oss << pixelComponentString(c.getUnmappedPixelComponentsProperty());
            oss << bitDepthString(c.getUnmappedPixelDepth());
            oss << ")\npremultiplication: ";
            oss << premultString(c.getPreMultiplication());
#ifdef OFX_EXTENSIONS_VEGAS
            oss << "\npixel order: ";
            oss << pixelOrderString(c.getPixelOrder());
#endif
            oss << "\nfield order: ";
            oss << fieldOrderString(c.getFieldOrder());
            oss << "\n";
            oss << (c.isConnected() ? "connected" : "not connected");
            oss << "\n";
            oss << (c.hasContinuousSamples() ? "continuous samples" : "discontinuous samples");
            oss << "\npixel aspect ratio: ";
            oss << c.getPixelAspectRatio();
            oss << "\nframe rate: ";
            oss << c.getFrameRate();
            oss << " (unmapped: ";
            oss << c.getUnmappedFrameRate();
            oss << ")";
            OfxRangeD range = c.getFrameRange();
            oss << "\nframe range: ";
            oss << range.min << "..." << range.max;
            oss << " (unmapped: ";
            range = c.getUnmappedFrameRange();
            oss << range.min << "..." << range.max;
            oss << ")";
            oss << "\nregion of definition: ";
            OfxRectD rod = c.getRegionOfDefinition(args.time);
            oss << rod.x1 << ' ' << rod.y1 << ' ' << rod.x2 << ' ' << rod.y2;
        }
        oss << "\n\n";
        oss << "Output: ";
        if (!dstClip_) {
            oss << "N/A";
        } else {
            OFX::Clip &c = *dstClip_;
            oss << pixelComponentString(c.getPixelComponentsProperty());
            oss << bitDepthString(c.getPixelDepth());
            oss << " (unmapped: ";
            oss << pixelComponentString(c.getUnmappedPixelComponentsProperty());
            oss << bitDepthString(c.getUnmappedPixelDepth());
            oss << ")\npremultiplication: ";
            oss << premultString(c.getPreMultiplication());
#ifdef OFX_EXTENSIONS_VEGAS
            oss << "\npixel order: ";
            oss << pixelOrderString(c.getPixelOrder());
#endif
            oss << "\nfield order: ";
            oss << fieldOrderString(c.getFieldOrder());
            oss << "\n";
            oss << (c.isConnected() ? "connected" : "not connected");
            oss << "\n";
            oss << (c.hasContinuousSamples() ? "continuous samples" : "discontinuous samples");
            oss << "\npixel aspect ratio: ";
            oss << c.getPixelAspectRatio();
            oss << "\nframe rate: ";
            oss << c.getFrameRate();
            oss << " (unmapped: ";
            oss << c.getUnmappedFrameRate();
            oss << ")";
            OfxRangeD range = c.getFrameRange();
            oss << "\nframe range: ";
            oss << range.min << "..." << range.max;
            oss << " (unmapped: ";
            range = c.getUnmappedFrameRange();
            oss << range.min << "..." << range.max;
            oss << ")";
            oss << "\nregion of definition: ";
            OfxRectD rod = c.getRegionOfDefinition(args.time);
            oss << rod.x1 << ' ' << rod.y1 << ' ' << rod.x2 << ' ' << rod.y2;
        }
        oss << "\n\n";
        oss << "time: " << args.time << ", renderscale: " << args.renderScale.x << 'x' << args.renderScale.y << '\n';

        sendMessage(OFX::Message::eMessageMessage, "", oss.str());
    }
}

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
bool
TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    if (!supportsRenderScale && (args.renderScale.x != 1. || args.renderScale.y != 1.)) {
        OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    if (srcClip_ && srcClip_->isConnected()) {
        rod = srcClip_->getRegionOfDefinition(args.time);
    } else {
        rod.x1 = rod.y1 = kOfxFlagInfiniteMin;
        rod.x2 = rod.y2 = kOfxFlagInfiniteMax;
    }
    return true;
}

//mDeclarePluginFactory(TestRenderPluginFactory, {}, {});
template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
class TestRenderPluginFactory : public OFX::PluginFactoryHelper<TestRenderPluginFactory<supportsTiles,supportsMultiResolution,supportsRenderScale> >
{
public:
    TestRenderPluginFactory(const std::string& id, unsigned int verMaj, unsigned int verMin):OFX::PluginFactoryHelper<TestRenderPluginFactory<supportsTiles,supportsMultiResolution,supportsRenderScale> >(id, verMaj, verMin){}
    virtual void load() {};
    virtual void unload() {};
    virtual void describe(OFX::ImageEffectDescriptor &desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context);
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context);
};


using namespace OFX;

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
void TestRenderPluginFactory<supportsTiles,supportsMultiResolution,supportsRenderScale>::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    std::string name = (std::string(kPluginName) + "_Ti" + (supportsTiles ? "OK" : "No")
                        +                          "_Mr" + (supportsMultiResolution ? "OK" : "No")
                        +                          "_Rs" + (supportsRenderScale ? "OK" : "No"));
    desc.setLabels(name, name, name);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    // add the supported contexts
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedContext(eContextGenerator);

    // add supported pixel depths
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);

    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(supportsMultiResolution);
    desc.setSupportsTiles(supportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);

}

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
void TestRenderPluginFactory<supportsTiles,supportsMultiResolution,supportsRenderScale>::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    // Source clip only in the filter context
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(true);

    if (context == eContextGeneral || context == eContextPaint) {
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(false);
        if (context == eContextGeneral) {
            maskClip->setOptional(true);
        }
        maskClip->setSupportsTiles(true);
        maskClip->setIsMask(true);
    }

    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");

    RGBAParamDescriptor *color0 = desc.defineRGBAParam(kParamColor0);
    color0->setLabels(kParamColor0Label, kParamColor0Label, kParamColor0Label);
    color0->setHint(kParamColor0Hint);
    color0->setDefault(0.0, 1.0, 1.0, 1.0);
    color0->setAnimates(true); // can animate
    page->addChild(*color0);

    RGBAParamDescriptor *color1 = desc.defineRGBAParam(kParamColor1);
    color1->setLabels(kParamColor1Label, kParamColor1Label, kParamColor1Label);
    color1->setHint(kParamColor1Hint);
    color1->setDefault(1.0, 0.0, 1.0, 1.0);
    color1->setAnimates(true); // can animate
    page->addChild(*color1);

    RGBAParamDescriptor *color2 = desc.defineRGBAParam(kParamColor2);
    color2->setLabels(kParamColor2Label, kParamColor2Label, kParamColor2Label);
    color2->setHint(kParamColor2Hint);
    color2->setDefault(1.0, 1.0, 0.0, 1.0);
    color2->setAnimates(true); // can animate
    page->addChild(*color2);

    RGBAParamDescriptor *color3 = desc.defineRGBAParam(kParamColor3);
    color3->setLabels(kParamColor3Label, kParamColor3Label, kParamColor3Label);
    color3->setHint(kParamColor3Hint);
    color3->setDefault(1.0, 0.0, 0.0, 1.0);
    color3->setAnimates(true); // can animate
    page->addChild(*color3);

    RGBAParamDescriptor *color4 = desc.defineRGBAParam(kParamColor4);
    color4->setLabels(kParamColor4Label, kParamColor4Label, kParamColor4Label);
    color4->setHint(kParamColor4Hint);
    color4->setDefault(0.0, 1.0, 0.0, 1.0);
    color4->setAnimates(true); // can animate
    page->addChild(*color4);

    RGBAParamDescriptor *color5 = desc.defineRGBAParam(kParamColor5);
    color5->setLabels(kParamColor5Label, kParamColor5Label, kParamColor5Label);
    color5->setHint(kParamColor5Hint);
    color5->setDefault(0.0, 0.0, 1.0, 1.0);
    color5->setAnimates(true); // can animate
    page->addChild(*color5);


    BooleanParamDescriptor *identityEven = desc.defineBooleanParam(kParamIdentityEven);
    identityEven->setLabels(kParamIdentityEvenLabel, kParamIdentityEvenLabel, kParamIdentityEvenLabel);
    identityEven->setHint(kParamIdentityEvenHint);
    identityEven->setDefault(false);
    identityEven->setAnimates(false);
    page->addChild(*identityEven);

    BooleanParamDescriptor *identityOdd = desc.defineBooleanParam(kParamIdentityOdd);
    identityOdd->setLabels(kParamIdentityOddLabel, kParamIdentityOddLabel, kParamIdentityOddLabel);
    identityOdd->setHint(kParamIdentityOddHint);
    identityOdd->setDefault(false);
    identityOdd->setAnimates(false);
    page->addChild(*identityOdd);

    BooleanParamDescriptor *forceCopy = desc.defineBooleanParam(kParamForceCopy);
    forceCopy->setLabels(kParamForceCopyLabel, kParamForceCopyLabel, kParamForceCopyLabel);
    forceCopy->setHint(kParamForceCopyHint);
    forceCopy->setDefault(false);
    forceCopy->setAnimates(false);
    page->addChild(*forceCopy);

    PushButtonParamDescriptor *clipInfo = desc.definePushButtonParam(kParamClipInfo);
    clipInfo->setLabels(kParamClipInfoLabel, kParamClipInfoLabel, kParamClipInfoLabel);
    clipInfo->setHint(kParamClipInfoHint);
    page->addChild(*clipInfo);

    ofxsMaskMixDescribeParams(desc, page);
}

template<bool supportsTiles, bool supportsMultiResolution, bool supportsRenderScale>
OFX::ImageEffect* TestRenderPluginFactory<supportsTiles,supportsMultiResolution,supportsRenderScale>::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new TestRenderPlugin<supportsTiles,supportsMultiResolution,supportsRenderScale>(handle);
}


void getTestRenderPluginID(OFX::PluginFactoryArray &ids)
{
    {
        std::string id = kPluginIdentifier"_TiOK_MrOK_RsOK";
        static TestRenderPluginFactory<true,true,true> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiOK_MrOK_RsNo";
        static TestRenderPluginFactory<true,true,false> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiOK_MrNo_RsOK";
        static TestRenderPluginFactory<true,false,true> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiOK_MrNo_RsNo";
        static TestRenderPluginFactory<true,false,false> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiNo_MrOK_RsOK";
        static TestRenderPluginFactory<false,true,true> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiNo_MrOK_RsNo";
        static TestRenderPluginFactory<false,true,false> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiNo_MrNo_RsOK";
        static TestRenderPluginFactory<false,false,true> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
    {
        std::string id = kPluginIdentifier"_TiNo_MrNo_RsNo";
        static TestRenderPluginFactory<false,false,false> p(id, kPluginVersionMajor, kPluginVersionMinor);
        ids.push_back(&p);
    }
}

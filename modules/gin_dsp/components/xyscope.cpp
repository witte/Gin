/*
  ==============================================================================

  This file is based on part of the dRowAudio JUCE module
  Copyright 2004-13 by dRowAudio.
  dRowAudio is provided under the terms of The MIT License (MIT):

  ==============================================================================
*/


XYScope::XYScope (AudioFifo& f) :
    fifo (f)
{
    setColour (lineColourId, Colours::white);
    setColour (backgroundColourId, Colours::black);
    setColour (traceColourId, Colours::white);
    
    channel.xBuffer.clear ((size_t) channel.bufferSize);
    channel.yBuffer.clear ((size_t) channel.bufferSize);

    startTimerHz (60);
}

XYScope::~XYScope()
{
    stopTimer();
}

void XYScope::setNumSamplesPerPixel (const float newNumSamplesPerPixel)
{
    numSamplesPerPixel = newNumSamplesPerPixel;
}

void XYScope::setZoomFactor (const float newZoomFactor)
{
    zoomFactor = newZoomFactor;
}

void XYScope::addSamples (const AudioSampleBuffer& buffer)
{
    jassert (buffer.getNumChannels() == 2);
    
    const int numSamples = buffer.getNumSamples();
    
    // if we don't have enough space in the fifo, clear out some old samples
    const int numFreeInBuffer = channel.samplesToProcess.getFreeSpace();
    if (numFreeInBuffer < numSamples)
        channel.samplesToProcess.ensureFreeSpace (numSamples);
    
    channel.samplesToProcess.write (buffer);

    needToUpdate = true;
}

//==============================================================================

void XYScope::paint (Graphics& g)
{
    if (needToUpdate)
    {
        needToUpdate = false;
        processPendingSamples();
    }
    
    render (g);
    
    g.setColour (findColour (lineColourId));
    g.drawRect (getLocalBounds());
    
    g.setColour (findColour (lineColourId).withMultipliedAlpha (0.5f));
}

void XYScope::timerCallback()
{
    while (fifo.getNumReady() > 0)
    {
        ScratchBuffer buffer (2, jmin (512, fifo.getNumReady()));
        
        fifo.read (buffer);
        addSamples (buffer);
        repaint();
    }
}

//==============================================================================
void XYScope::processPendingSamples()
{
    int numSamples = channel.samplesToProcess.getNumReady();
    
    ScratchBuffer tempProcessingBlock (2, numSamples);
    channel.samplesToProcess.read (tempProcessingBlock);
    
    auto samplesL = tempProcessingBlock.getReadPointer (0);
    auto samplesR = tempProcessingBlock.getReadPointer (1);

    while (--numSamples >= 0)
    {
        const float currentSampleL = *samplesL++;
        const float currentSampleR = *samplesR++;
        
        channel.currentX += currentSampleL;
        channel.currentY += currentSampleR;
        channel.numAveraged++;

        if (--channel.numLeftToAverage <= 0)
        {
            channel.xBuffer[channel.bufferWritePos] = channel.currentX / channel.numAveraged;
            channel.yBuffer[channel.bufferWritePos] = channel.currentY / channel.numAveraged;

            channel.numAveraged = 0;
            channel.currentX = 0.0;
            channel.currentY = 0.0;

            ++channel.bufferWritePos %= channel.bufferSize;
            channel.numLeftToAverage += jmax (1.0f, numSamplesPerPixel);
        }
    }
}

void XYScope::render (Graphics& g)
{
    g.fillAll (Colours::transparentBlack);

    const int w = getWidth();
    const int h = getHeight();

    int curPoint = 0;
    int points = 50;
    int bufferReadPos = channel.bufferWritePos - points;
    
    Path p;
    
    int pos = bufferReadPos;
    
    g.setColour (findColour (traceColourId).withMultipliedAlpha (0.5f));
    
    while (curPoint < points)
    {
        pos++;
        if (pos == channel.bufferSize)
            pos = 0;

        const float y = (1.0f - (0.5f + (0.5f * zoomFactor * (channel.yBuffer[pos])))) * h;
        const float x = (1.0f - (0.5f + (0.5f * zoomFactor * (channel.xBuffer[pos])))) * w;

        if (curPoint == 0)
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
        
        curPoint++;
    }
    
    g.setColour (findColour (traceColourId ));
    p = p.createPathWithRoundedCorners (10.0f);
    g.strokePath (p, PathStrokeType (1.5f));
}

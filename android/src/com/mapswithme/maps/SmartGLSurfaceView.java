package com.mapswithme.maps;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;

public class SmartGLSurfaceView extends GLSurfaceView
{
  private static String TAG = "SmartGLSurfaceView";

  private SmartRenderer m_renderer;

  // Commands which are executed on the renderer thread
  private Runnable m_tryToLoadResourcesIfReady;
  private Runnable m_unloadResources;

  public SmartGLSurfaceView(Context context)
  {
    super(context);
    init();
  }

  public SmartGLSurfaceView(Context context, android.util.AttributeSet attrs)
  {
    super(context, attrs);
    init();
  }

  private void init()
  {
    setEGLConfigChooser(true);
    m_renderer = new SmartRenderer();
    setRenderer(m_renderer);
    setRenderMode(RENDERMODE_WHEN_DIRTY);

    m_tryToLoadResourcesIfReady = new Runnable()
    {
      @Override
      public void run() { m_renderer.loadResources(); }
    };

    m_unloadResources = new Runnable()
    {
      @Override
      public void run() { m_renderer.unloadResources(); }
    };
  }

  @Override
  public void surfaceCreated (SurfaceHolder holder)
  {
    Log.d(TAG, "surfaceCreated");
    m_renderer.m_isBaseSurfaceReady = false;
    nativeSetRedraw(false);
    super.surfaceCreated(holder);
  }

  @Override
  public void surfaceChanged (SurfaceHolder holder, int format, int w, int h)
  {
    Log.d(TAG, "surfaceChanged " + w + " " + h);
    m_renderer.m_isBaseSurfaceReady = true;
    nativeSetRedraw(true);
    super.surfaceChanged(holder, format, w, h);
    queueEvent(m_tryToLoadResourcesIfReady);
  }

  @Override
  public void surfaceDestroyed (SurfaceHolder holder)
  {
    Log.d(TAG, "surfaceDestroyed");
    m_renderer.m_isBaseSurfaceReady = false;
    m_renderer.m_isLocalSurfaceReady = false;
    nativeSetRedraw(false);
    super.surfaceDestroyed(holder);
    queueEvent(m_unloadResources);
  }

  @Override
  public void onResume()
  {
    m_renderer.m_isActive = true;
    super.onResume();
    queueEvent(m_tryToLoadResourcesIfReady);
    requestRender();
  }

  @Override
  public void onPause()
  {
    m_renderer.m_isActive = false;
    super.onPause();
    queueEvent(m_unloadResources);
  }

  // Should be called from parent activity
  public void onStop()
  {
    queueEvent(m_unloadResources);
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus)
  {
    m_renderer.m_isFocused = hasFocus;
    if (hasFocus)
    {
      queueEvent(m_tryToLoadResourcesIfReady);
      requestRender();
    }
  }

  @Override
  public boolean onTouchEvent (MotionEvent event)
  {
    switch (event.getAction() & MotionEvent.ACTION_MASK)
    {
    case MotionEvent.ACTION_DOWN:
      nativeMove(0, event.getX(), event.getY());
      break;

    case MotionEvent.ACTION_POINTER_DOWN:
      nativeZoom(0, event.getX(0), event.getY(0), event.getX(1), event.getY(1));
      break;

    case MotionEvent.ACTION_MOVE:
      if (event.getPointerCount() > 1)
        nativeZoom(1, event.getX(0), event.getY(0), event.getX(1), event.getY(1));
      else
        nativeMove(1, event.getX(), event.getY());
      break;

    case MotionEvent.ACTION_POINTER_UP:
      nativeZoom(2, event.getX(0), event.getY(0), event.getX(1), event.getY(1));
      break;

    case MotionEvent.ACTION_UP:
      nativeMove(2, event.getX(), event.getY());
      break;

    case MotionEvent.ACTION_CANCEL:
      if (event.getPointerCount() > 1)
        nativeZoom(2, event.getX(0), event.getY(0), event.getX(1), event.getY(1));
      else
        nativeMove(2, event.getX(), event.getY());
    }

    requestRender();
    return true;
  }

  // Mode 0 - Start, 1 - Do, 2 - Stop
  private native void nativeMove(int mode, float x, float y);
  private native void nativeZoom(int mode, float x1, float y1, float x2, float y2);
  private native void nativeSetRedraw(boolean isValid);
}

class SmartRenderer implements GLSurfaceView.Renderer
{
  private static String TAG = "SmartRenderer";

  // These flags are modified from GUI thread
  public boolean m_isFocused = false;
  public boolean m_isActive = false;
  public boolean m_isBaseSurfaceReady = false;
  // This flag is modified from Renderer thread
  public boolean m_isLocalSurfaceReady = false;

  private boolean m_areResourcesLoaded = false;

  public SmartRenderer()
  {
  }

  public void loadResources()
  {
    // We can load resources even if we don't have the focus
    if (!m_areResourcesLoaded && m_isActive && m_isBaseSurfaceReady && m_isLocalSurfaceReady)
    {
      Log.d(TAG, "***loadResources***");
      nativeLoadResources();
      m_areResourcesLoaded = true;
    }
  }

  public void unloadResources()
  {
    if (m_areResourcesLoaded)
    {
      Log.d(TAG, "***unloadResources***");
      nativeUnloadResources();
      m_areResourcesLoaded = false;
    }
  }

  @Override
  public void onDrawFrame(GL10 gl)
  {
    Log.d(TAG, "onDrawFrame");
    if (m_isActive && m_isLocalSurfaceReady && m_isBaseSurfaceReady)
    {
      if (!m_areResourcesLoaded)
        loadResources();
      if (m_isFocused)
      {
        Log.d(TAG, "***Draw***");
        nativeDrawFrame();
      }
    }
  }

  @Override
  public void onSurfaceChanged(GL10 gl, int width, int height)
  {
    Log.d(TAG, "onSurfaceChanged " + width + " " + height);
    m_isLocalSurfaceReady = true;
    loadResources();
    nativeResize(width, height);
  }

  @Override
  public void onSurfaceCreated(GL10 gl, EGLConfig config)
  {
    Log.d(TAG, "onSurfaceCreated");
    m_isLocalSurfaceReady = false;
  }

  private native void nativeLoadResources();
  private native void nativeUnloadResources();
  private native void nativeDrawFrame();
  private native void nativeResize(int width, int height);
}

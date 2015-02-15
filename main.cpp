#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ShaderEditor.h"
#include "Renderer.h"
#include "FFT.h"
#include "MIDI.h"
#include "Timer.h"
#include "external/scintilla/src/UniConversion.h"
#include "external/jsonxx/jsonxx.h"

int main()
{
  RENDERER_SETTINGS settings;
  settings.bVsync = false;
#ifdef _DEBUG
  settings.nWidth = 1280;
  settings.nHeight = 720;
  settings.windowMode = RENDERER_WINDOWMODE_WINDOWED;
#else
  settings.nWidth = 1920;
  settings.nHeight = 1080;
  settings.windowMode = RENDERER_WINDOWMODE_FULLSCREEN;
  if (!Renderer::OpenSetupDialog( &settings ))
    return -1;
#endif

  if (!Renderer::Open( &settings ))
    return -1;

  if (!FFT::Open())
    return -1;

  if (!MIDI::Open())
    return -1;

  std::map<std::string,Renderer::Texture*> textures;
  std::map<int,std::string> midiRoutes;

  int nFontSize = 16;
#ifdef _WIN32
  std::string sFontPath = "c:\\Windows\\Fonts\\cour.ttf";
#else
  std::string sFontPath = "/usr/share/fonts/corefonts/cour.ttf";
#endif
  unsigned char nOpacity = 0xC0;

  int nDebugOutputHeight = 200;
  int nTexPreviewWidth = 64;

  char szConfig[65535];
  FILE * fConf = fopen("config.json","rb");
  if (fConf)
  {
    memset( szConfig, 0, 65535 );
    int n = fread( szConfig, 1, 65535, fConf );
    fclose(fConf);

    jsonxx::Object o;
    o.parse( szConfig );

    if (o.has<jsonxx::Object>("textures"))
    {
      printf("Loading textures...\n");
      std::map<std::string, jsonxx::Value*> tex = o.get<jsonxx::Object>("textures").kv_map();
      for (std::map<std::string, jsonxx::Value*>::iterator it = tex.begin(); it != tex.end(); it++)
      {
        char * fn = (char*)it->second->string_value_->c_str();
        printf("* %s...\n",fn);
        Renderer::Texture * tex = Renderer::CreateRGBA8TextureFromFile( fn );
        textures.insert( std::make_pair( it->first, tex ) );
      }
    }
    if (o.has<jsonxx::Object>("font"))
    {
      if (o.get<jsonxx::Object>("font").has<jsonxx::Number>("size"))
        nFontSize = o.get<jsonxx::Object>("font").get<jsonxx::Number>("size");
      if (o.get<jsonxx::Object>("font").has<jsonxx::String>("file"))
        sFontPath = o.get<jsonxx::Object>("font").get<jsonxx::String>("file");
    }
    if (o.has<jsonxx::Object>("gui"))
    {
      if (o.get<jsonxx::Object>("gui").has<jsonxx::Number>("outputHeight"))
        nDebugOutputHeight = o.get<jsonxx::Object>("gui").get<jsonxx::Number>("outputHeight");
      if (o.get<jsonxx::Object>("gui").has<jsonxx::Number>("texturePreviewWidth"))
        nTexPreviewWidth = o.get<jsonxx::Object>("gui").get<jsonxx::Number>("texturePreviewWidth");
      if (o.get<jsonxx::Object>("gui").has<jsonxx::Number>("opacity"))
        nOpacity = o.get<jsonxx::Object>("gui").get<jsonxx::Number>("opacity");
    }
    if (o.has<jsonxx::Object>("midi"))
    {
      std::map<std::string, jsonxx::Value*> tex = o.get<jsonxx::Object>("midi").kv_map();
      for (std::map<std::string, jsonxx::Value*>::iterator it = tex.begin(); it != tex.end(); it++)
      {
        midiRoutes.insert( std::make_pair( it->second->number_value_, it->first ) );
      }
    }
  }

  Renderer::Texture * texFFT = Renderer::Create1DR32Texture( FFT_SIZE );

  bool shaderInitSuccessful = false;
  char szShader[65535];
  char szError[4096];
  FILE * f = fopen(Renderer::defaultShaderFilename,"rb");
  if (f)
  {
    memset( szShader, 0, 65535 );
    int n = fread( szShader, 1, 65535, f );
    fclose(f);
    if (Renderer::ReloadShader( szShader, strlen(szShader), szError, 4096 ))
    {
      shaderInitSuccessful = true;
    }
  }
  if (!shaderInitSuccessful)
  {
    strncpy( szShader, Renderer::defaultShader, 65535 );
    if (!Renderer::ReloadShader( szShader, strlen(szShader), szError, 4096 ))
    {
      puts(szError);
      assert(0);
    }
  }

#ifdef SCI_LEXER
  Scintilla_LinkLexers();
#endif
  Scintilla::Surface * surface = Scintilla::Surface::Allocate( SC_TECHNOLOGY_DEFAULT );
  surface->Init( NULL );

  int nMargin = 20;

  bool bTexPreviewVisible = true;

  SHADEREDITOR_OPTIONS options;
  options.sFontPath = sFontPath;
  options.nFontSize = nFontSize;
  options.nOpacity = nOpacity;
  options.rect = Scintilla::PRectangle( nMargin, nMargin, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin * 2 - nDebugOutputHeight );
  ShaderEditor mShaderEditor( surface );
  mShaderEditor.Initialise( options );
  mShaderEditor.SetText( szShader );

  options.rect = Scintilla::PRectangle( nMargin, settings.nHeight - nMargin - nDebugOutputHeight, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin );
  ShaderEditor mDebugOutput( surface );
  mDebugOutput.Initialise( options );
  mDebugOutput.SetText( "" );
  mDebugOutput.SetReadOnly(true);

  bool bShowGui = true;
  Timer::Start();
  float fNextTick = 0.1;
  while (!Renderer::WantsToQuit())
  {
    float time = Timer::GetTime() / 1000.0; // seconds
    Renderer::StartFrame();

    
    for(int i=0; i<Renderer::mouseEventBufferCount; i++)
    {
      if (bShowGui)
      {
        switch (Renderer::mouseEventBuffer[i].eventType)
        {
          case Renderer::MOUSEEVENTTYPE_MOVE:
            mShaderEditor.ButtonMovePublic( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ) );
            break;
          case Renderer::MOUSEEVENTTYPE_DOWN:
            mShaderEditor.ButtonDown( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ), time * 1000, false, false, false );
            break;
          case Renderer::MOUSEEVENTTYPE_UP:
            mShaderEditor.ButtonUp( Scintilla::Point( Renderer::mouseEventBuffer[i].x, Renderer::mouseEventBuffer[i].y ), time * 1000, false );
            break;
        }
      }
    }
    Renderer::mouseEventBufferCount = 0;

    for(int i=0; i<Renderer::keyEventBufferCount; i++)
    {
      if (Renderer::keyEventBuffer[i].scanCode == 283) // F2
      {
        if (bTexPreviewVisible)
        {
          mShaderEditor.SetPosition( Scintilla::PRectangle( nMargin, nMargin, settings.nWidth - nMargin, settings.nHeight - nMargin * 2 - nDebugOutputHeight ) );
          mDebugOutput .SetPosition( Scintilla::PRectangle( nMargin, settings.nHeight - nMargin - nDebugOutputHeight, settings.nWidth - nMargin, settings.nHeight - nMargin ) );
          bTexPreviewVisible = false;
        }
        else
        {
          mShaderEditor.SetPosition( Scintilla::PRectangle( nMargin, nMargin, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin * 2 - nDebugOutputHeight ) );
          mDebugOutput .SetPosition( Scintilla::PRectangle( nMargin, settings.nHeight - nMargin - nDebugOutputHeight, settings.nWidth - nMargin - nTexPreviewWidth - nMargin, settings.nHeight - nMargin ) );
          bTexPreviewVisible = true;
        }
      }
      else if (Renderer::keyEventBuffer[i].scanCode == 286) // F5
      {
        mShaderEditor.GetText(szShader,65535);
        if (Renderer::ReloadShader( szShader, strlen(szShader), szError, 4096 ))
        {
          FILE * f = fopen(Renderer::defaultShaderFilename,"wb");
          fwrite( szShader, strlen(szShader), 1, f );
          fclose(f);
          mDebugOutput.SetText( "" );
        }
        else
        {
          mDebugOutput.SetText( szError );
        }
      }
      else if (Renderer::keyEventBuffer[i].scanCode == 292) // F11
      {
        bShowGui = !bShowGui;
      }
      else if (bShowGui)
      {
        bool consumed = false;
        if (Renderer::keyEventBuffer[i].scanCode)
        {
        mShaderEditor.KeyDown(
          iswalpha(Renderer::keyEventBuffer[i].scanCode) ? towupper(Renderer::keyEventBuffer[i].scanCode) : Renderer::keyEventBuffer[i].scanCode,
          Renderer::keyEventBuffer[i].shift,
          Renderer::keyEventBuffer[i].ctrl, 
          Renderer::keyEventBuffer[i].alt,
          &consumed);
        }
        if (!consumed && Renderer::keyEventBuffer[i].character)
        {
          char    utf8[5] = {0,0,0,0,0};
          wchar_t utf16[2] = {Renderer::keyEventBuffer[i].character, 0};
          Scintilla::UTF8FromUTF16(utf16, 1, utf8, 4 * sizeof(char));
          mShaderEditor.AddCharUTF(utf8, strlen(utf8));
        }

      }
    }
    Renderer::keyEventBufferCount = 0;

    Renderer::SetShaderConstant( "fGlobalTime", time );
    Renderer::SetShaderConstant( "v2Resolution", settings.nWidth, settings.nHeight );

    for (std::map<int,std::string>::iterator it = midiRoutes.begin(); it != midiRoutes.end(); it++)
    {
      Renderer::SetShaderConstant( (char*)it->second.c_str(), MIDI::GetCCValue( it->first ) );
    }


    static float fftData[FFT_SIZE];
    if (FFT::GetFFT(fftData))
      Renderer::UpdateR32Texture( texFFT, fftData );

    Renderer::SetShaderTexture( "texFFT", texFFT );

    for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
    {
      Renderer::SetShaderTexture( (char*)it->first.c_str(), it->second );
    }

    Renderer::RenderFullscreenQuad();

    Renderer::StartTextRendering();

    if (bShowGui)
    {
      if (time > fNextTick)
      {
        mShaderEditor.Tick();
        mDebugOutput.Tick();
        fNextTick = time + 0.1;
      }

      mShaderEditor.Paint();
      mDebugOutput.Paint();

      Renderer::SetTextRenderingViewport( Scintilla::PRectangle(0,0,Renderer::nWidth,Renderer::nHeight) );

      if (bTexPreviewVisible)
      {
        int y1 = nMargin;
        int x1 = settings.nWidth - nMargin - nTexPreviewWidth;
        int x2 = settings.nWidth - nMargin;
        for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
        {
          int y2 = y1 + nTexPreviewWidth * (it->second->width / (float)it->second->height);
          Renderer::BindTexture( it->second );
          Renderer::RenderQuad(
            Renderer::Vertex( x1, y1, 0xFFFFFFFF, 0.0, 0.0 ),
            Renderer::Vertex( x2, y1, 0xFFFFFFFF, 1.0, 0.0 ),
            Renderer::Vertex( x2, y2, 0xFFFFFFFF, 1.0, 1.0 ),
            Renderer::Vertex( x1, y2, 0xFFFFFFFF, 0.0, 1.0 )
          );
          surface->DrawTextNoClip( Scintilla::PRectangle(x1,y1,x2,y2), *mShaderEditor.GetTextFont(), y2 - 5.0, it->first.c_str(), it->first.length(), 0xffFFFFFF, 0x00000000);
          y1 = y2 + nMargin;
        }
      }

      char szHelp[] = "F2 - toggle texture preview   F5 - recompile shader   F11 - hide GUI";
      surface->DrawTextNoClip( Scintilla::PRectangle(20,Renderer::nHeight - 20,100,Renderer::nHeight), *mShaderEditor.GetTextFont(), Renderer::nHeight - 5.0, szHelp, strlen(szHelp), 0x80FFFFFF, 0x00000000);
    }


    Renderer::EndTextRendering();

    Renderer::EndFrame();
  }

  delete surface;

  MIDI::Close();
  FFT::Close();

  Renderer::ReleaseTexture( texFFT );
  for (std::map<std::string, Renderer::Texture*>::iterator it = textures.begin(); it != textures.end(); it++)
  {
    Renderer::ReleaseTexture( it->second );
  }

  Renderer::Close();
  return 0;
}

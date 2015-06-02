#include "stdafx.h"

using namespace Magick;

DECLARE_COMPONENT_VERSION("Image decoder", "1.1.0", "Open BMP, PNG and JPG images in foobar!\n\nBe sure to add the image file extensions to your media library exclusions or you will have a bad time.\n;*.jpg;*.png;*.bmp;*.jpeg;");

static const GUID samples_per_pixel_guid = { 0xcaa64014, 0x6c9c, 0x46b2, { 0x7b, 0xa7, 0xa, 0x5c, 0xbb, 0x10, 0x2f, 0xb6 } };
advconfig_integer_factory samples_per_pixel(
  "Image decoding spectrogram height",
  samples_per_pixel_guid,
  advconfig_branch::guid_branch_decoding,
  0.0,
  900,
  100,
  1200
  );

static const GUID amp_scale_guid = { 0xcacc0014, 0x6c9c, 0x46b3, { 0x7b, 0xa7, 0xf, 0x5c, 0xab, 0x10, 0x6f, 0xb6 } };
advconfig_integer_factory amp_scale(
  "Image decoding amplitude scale (1-1000)",
  amp_scale_guid,
  advconfig_branch::guid_branch_decoding,
  0.0,
  430,
  1,
  1000
  );

static const GUID lowpass_enabled_guid = { 0xfab55014, 0xc445, 0x8ff1, { 0xc3, 0xa7, 0xaa, 0x5c, 0x71, 0x7c, 0xb7, 0xe4 } };
advconfig_checkbox_factory lowpass_enabled(
  "Enable image decoding lowpass filter",
  lowpass_enabled_guid,
  advconfig_branch::guid_branch_decoding,
  0.0,
  true
  );


class img_type : public input_singletrack_impl {
  typedef pfc::array_t<uint8_t> BArray;
  std::string n;
  file_ptr myFile;
  t_filestats fs;
  int c;
  BArray raw;
  
  float **img = nullptr;
  audio_sample * buffer = nullptr;

  int w, h;
  float *phase = nullptr, *hz = nullptr;
  float bdw, vol;
  int spp;
  int xx;
  float argv;

  float *oamp = nullptr;

  //ILuint ilimg = 0;

  //Blob imBlob;
  Magick::Image imImg;

  float * sinlook = nullptr;

  static const float PI;
  static const int LOOKUP_SIZE = 6000;

  

  int get_int32(BArray arr, int at) {
    return (arr[at+3] << 24) | (arr[at+2] << 16) | (arr[at+1] << 8) | (arr[at]);
  }
  int get_int16(BArray arr, int at) {
    return (arr[at + 1] << 8) | (arr[at]);
  }

  float my_sin(float val) {
    float mult = val / (2*PI);
    mult -= (int)(mult);

    float rem = (LOOKUP_SIZE - 1) * mult;
    int idx = (int)(rem);
    float ret = sinlook[idx];
    return ret;
  }

public:
  void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort)  {

    input_open_file_helper(p_filehint, p_path, p_reason, p_abort);
    if(p_filehint == nullptr) {
      return;
    }
    myFile = p_filehint;
    n = std::string(p_path);
    myFile->read_till_eof(raw, p_abort);

    try {
      Blob imBlob = Blob((void*)raw.get_ptr(), raw.get_count());
      imImg = Magick::Image(imBlob);
    }
    catch (Exception imE) {
      throw exception_io_unsupported_format(imE.what());
    }


    w = imImg.columns();
    h = imImg.rows();
    spp = samples_per_pixel.get();
    spp = -0.7646 * (-spp - 332);
    vol = amp_scale.get();
    vol = 18189 * pow(2.71828, -9.45817 * (vol / 1000.0));
    if (vol <= 0)
      vol = 1;
    
    if (h > spp) {
      try {
        float rat = (float)w / h;
        Geometry geom((int)(spp*rat), spp);
        imImg.resize(geom);
        w = imImg.columns();
        h = imImg.rows();
      }
      catch (Exception imE) {}
    }


    bool dum = false;
    foobar2000_io::filesystem::g_get_stats(p_path, fs, dum, p_abort);
    hz = phase = nullptr; 
    
    

    sinlook = new float[LOOKUP_SIZE];
    for (int i = 0; i < LOOKUP_SIZE; i++) {
      float ang = (i / (float)(LOOKUP_SIZE - 1)) * 2 * PI;
      sinlook[i] = std::sinf(ang);
    }

    hz = new float[h];
    phase = new float[h];
    buffer = new audio_sample[spp];
    
    if (lowpass_enabled.get())
      oamp = new float[h];

    srand(time(NULL));
    bdw = 22000.0 / spp;

    /*bdw = 22000.0 / h;
    if (bdw > (22000.0 / spp))
      bdw = (22000.0 / spp);*/
    float logmul = 22000.0 / std::pow(h, 2.71828);
    for (int a = 0; a < h; a++) {
      phase[a] = ((rand() % 20000) / 19999.0) * 2 * PI;
      //phase[a] = ((1140671485 * a + 12820163) % 10000) / 10000.0 * 2 * PI;
      //phase[a] = ((float)(a % 12) / h) *PI/6;
      hz[a] = bdw * (h - a - 1);

      if (oamp != nullptr)
        oamp[a] = 0;
    }
    xx = 0;

    console::printf("foo_img::open: %s", p_path);

  }


  void get_info(file_info & p_info,abort_callback & p_abort) {
    p_info.info_set_int("samplerate", 44100);
    p_info.info_set_int("channels", 1);
    p_info.info_set_int("bitspersample", 32);
    p_info.info_set("codec", "PCM");
    p_info.info_set("encoding", "lossless");
    p_info.set_length((w * spp) / 44100.0);
    p_info.info_set_bitrate(44100 * 32);
    
    int p = n.rfind('\\');
    p_info.meta_add("TITLE", n.substr(p+1).c_str());
  }

  t_filestats get_file_stats(abort_callback & p_abort) {
    return fs;
  }

  void decode_initialize(unsigned p_flags,abort_callback & p_abort) {
    console::printf("foo_img::initialize: %s", n.c_str());
    int w = this->w;
    int h = this->h;
    
    img = new float*[w];
    float _min = 1.0, _max = 0.0;

    Pixels cache(imImg);
    PixelPacket *pix;
    pix = cache.get(0, 0, w, h);
    
    for (int x = 0; x < w; x++) {
      img[x] = new float[h];
      for (int y = h - 1; y >= 0; y--) {
        PixelPacket pp = pix[y*w + x];

        //float v = ((pow(pp.red / 65535.0, 2.2) * 0.2989 + pow(pp.green / 65535.0, 2.2) * 0.5870 + pow(pp.blue / 65535.0, 2.2) * 0.1140));
        float v = (pp.red *  0.2989 + pp.green *  0.5870 + pp.blue *  0.1140) / 65535;
        img[x][y] = v;

        if (v < _min)
          _min = v;
        else if (v > _max)
          _max = v;
      }
    }
    

    float scale = (_max-_min);

    for (int x = 0; x < w; x++) {
      float *c = img[x];
      for (int y = 0; y < h; y++) {
        c[y] = ((c[y] - _min) * scale);
      }
    }
    

  }

  bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort) {
    if(xx >= w)
      return false;

    float *col = img[xx];
    int w = this->w;
    int h = this->h;
    int xx = this->xx;
    int spp = this->spp;

    float max = 0;

    int pos = xx * spp;
    for(int s = 0; s < spp; s++) {
      float sum = 0;
      int smp = pos + s;
      float arg = smp * PI * 2 / 44100.0;
      for(int y = 0; y < h; y++) {
        float p = col[y];
        float amp = (p * 0.2 + p*p * 0.8);
        if (oamp != nullptr) {
          amp = oamp[y] * 0.92 + amp * 0.08;
          oamp[y] = amp;
        }
        float pt = my_sin(hz[y] * arg + phase[y]);
        sum += pt*amp;
      }
      sum = sum / vol;
      if(sum < -1.0)
        sum = -1.0;
      else if(sum > 1.0)
        sum = 1.0;
        
      buffer[s] = sum;
    }

    this->xx++;

    p_chunk.set_data(buffer, spp, 1, 44100);
    return true;
  }

  void decode_seek(double p_seconds,abort_callback & p_abort) {
    double samps = p_seconds * 44100;
    samps /= spp;
    xx = (int)samps;
  }

  bool decode_can_seek() {
    return true;
  }

  bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta){
    return false;
  }

  bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta){
    return false;
  }

  void decode_on_idle(abort_callback & p_abort){
  }

  void retag(const file_info & p_info,abort_callback & p_abort){
    // don't care
  }
  
  static bool endswith(const char * s1, const char * s2) {
    return strlen(s1) >= strlen(s2) && stricmp(s1 + strlen(s1) - strlen(s2), s2) == 0;
  }
  static bool startswith(const char * s1, const char * s2) {
    return strlen(s1) >= strlen(s2) && strnicmp(s1, s2, strlen(s2)) == 0;
  }


  static bool g_is_our_content_type(const char * p_content_type){
    return stricmp(p_content_type, "image/bmp") == 0 ||
      stricmp(p_content_type, "image/png") == 0 ||
      stricmp(p_content_type, "image/jpeg") == 0;
  }
  static bool g_is_our_path(const char * p_path,const char * p_extension) {
    if (strncmp(p_path, "unpack:", strlen("unpack:")) == 0) {
      return false; // don't open packs
    }

    const char * fname = strrchr(p_path, '\\') + 1;

    if (startswith(fname, "albumart") ||
      startswith(fname, "cover") ||
      startswith(fname, "thumb") ||
      startswith(fname, "folder") ||
      startswith(fname, "front") ||
      startswith(fname, "disc") ||
      startswith(fname, "back"))
      return false;
    

    return stricmp(p_extension, "bmp") == 0 || 
      stricmp(p_extension, "jpg") == 0 ||
      stricmp(p_extension, "jpeg") == 0 ||
      stricmp(p_extension, "png") == 0;
  }


  ~img_type(){
    delete hz;
    delete phase;
    delete buffer;
    delete sinlook;
    delete oamp;

    if (img != nullptr) {
      for (int i = 0; i < w; i++)
        delete img[i];
      delete img;
    }
  }
};

const float img_type::PI = 3.14159265359;

input_singletrack_factory_t<img_type> fac;



std::string wstrtostr(const std::wstring &wstr)
{
  if (wstr.empty()) return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
  std::string strTo(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
  return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring strtowstr(const std::string &str)
{
  if (str.empty()) return std::wstring();
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
  return wstrTo;
}


BOOLEAN WINAPI DllMain(IN HINSTANCE hDllHandle,
  IN DWORD     nReason,
  IN LPVOID    Reserved)
{
  BOOLEAN bSuccess = TRUE;


  //  Perform global initialization.

  switch (nReason)
  {
  case DLL_PROCESS_ATTACH:

    //  For optimization.

    DisableThreadLibraryCalls(hDllHandle);

    break;

  case DLL_PROCESS_DETACH:

    break;
  }

  LPSTR s = GetCommandLineA();
  InitializeMagick(s);

  return bSuccess;

}
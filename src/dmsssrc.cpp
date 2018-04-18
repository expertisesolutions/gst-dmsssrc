
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>

#include <boost/algorithm/string.hpp>

#include <boost/fusion/include/vector.hpp>
#include <boost/spirit/home/x3.hpp>

#include <openssl/md5.h>

#include <dahua-protocol/connection.hpp>

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <string>
#include <iostream>
#include <cassert>
#include <thread>
#include <condition_variable>
#include <deque>

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
// static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
//     GST_PAD_SINK,
//     GST_PAD_ALWAYS,
//     GST_STATIC_CAPS ("ANY")
//     );

enum
{
  PROP_0,
  PROP_URL,
  PROP_PORT,
  PROP_USERNAME,
  PROP_PASSWORD
};

struct GstDmssSrc : GstPushSrc
{
  gboolean query (GstBaseSrc * bsrc, GstQuery * query);
  GstCaps* fixate(GstBaseSrc * bsrc, GstCaps* caps);


  GstDmssSrc()
    : ready(false)
    , hostname("192.168.1.108"), username("admin"), password("admin")
    , offset(0), next_dhav_packet(true), dhav_packet_size(0u), dhav_packet_offset(0u)
  {
  }
  
  std::thread work_thread;
  std::string hostname;
  std::string username, password;
  unsigned short port;
  std::condition_variable condvar;
  bool ready;
  std::mutex mutex;

  int offset;
  bool next_dhav_packet; // waiting for new packet
  std::size_t dhav_packet_size;
  std::size_t dhav_packet_offset;
  std::deque<std::vector<char>> packets;
  std::vector<char> dhav_last_packet;

  void finished_dhav()
  {
    std::cout << "Finished dhav packet" << std::endl;
    std::unique_lock<std::mutex> l(mutex);
    std::vector<char> p{std::next(dhav_last_packet.begin(), 16), std::prev(dhav_last_packet.end(), 4)};
    dhav_last_packet.clear();
    packets.emplace_back(std::move(p));
  }

  void get_buffer(std::string_view buffer)
  {
    std::unique_lock<std::mutex> l(mutex);
    if(!ready)
    {
      ready = true;
      condvar.notify_all();
    }
    l.unlock();
    
    char dhav[4] = {'D', 'H', 'A', 'V'};
    
    auto current = buffer.begin()
      , last = buffer.end();

    while(current != last)
    {
      std::cout << "loop " << std::distance(current, last) << std::endl;
      if(next_dhav_packet)
      {
      auto begin_buffer = current;
      std::cout << "next_packet" << std::endl;
      // search dhav packet header
      while(std::distance(current, last) > 16
            && (*std::next(current, 0) != dhav[0]
                || *std::next(current, 1) != dhav[1]
                || *std::next(current, 2) != dhav[2]
                || *std::next(current, 3) != dhav[3]))
        ++current;

      if(std::distance(begin_buffer, current))
        std::cout << "thrown away " << std::distance(begin_buffer, current) << std::endl;

      if(std::distance(current, last) > 16) // found beginning of dhav packet
      {
        dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)128));

        dhav_packet_size = *((std::uint32_t*)((&*current + 12)));
        // dhav_packet_size = *std::next(current, 12);
        // dhav_packet_size += *std::next(current, 13) << 8;
        // dhav_packet_size += *std::next(current, 14) << 16;
        // dhav_packet_size += *std::next(current, 15) << 24;
        dhav_packet_offset = 0u;

        // std::advance(current, 16);
          
        std::cout << "dhav size " << dhav_packet_size << " remaining buffer size " << std::distance(current+16, last) << std::endl;

        bool partial = std::distance(current, last) < dhav_packet_size;
        unsigned int buffer_size = std::min((std::ptrdiff_t)dhav_packet_size, std::distance(current, last));
          
        std::vector<char> v(current, std::next(current, buffer_size));
        std::swap(v, dhav_last_packet);
        current += buffer_size;
        if(!partial)
        {
          std::cout << "not partial" << std::endl;
          dahua_ptc::print_packet(current - 4, std::min(std::distance(current - 4, last), (std::ptrdiff_t)128));
          int end_dhav_packet_size = *(std::uint32_t*)(&*current - 4);
          assert(end_dhav_packet_size == dhav_packet_size);

          finished_dhav();
        }
        else
        {
          std::cout << "partial" << std::endl;
          next_dhav_packet = false;
        }

        dhav_packet_offset = buffer_size;

        // l.lock();
        // packets.emplace_back(std::move(v));
        // l.unlock();
      }
      }
      else
      {
        std::cout << "middle of DHAV packet offset: " << dhav_packet_offset << " size " << dhav_packet_size << std::endl;
        bool partial = dhav_packet_offset + std::distance(current, last) < dhav_packet_size;
        std::size_t buffer_size = std::min((std::ptrdiff_t)(dhav_packet_size - dhav_packet_offset)
                                             , std::distance(current, last));
        std::cout << "buffer size " << buffer_size << " partial " << partial << std::endl;
        dhav_last_packet.insert(dhav_last_packet.end(), current, std::next(current, buffer_size));
        std::advance(current, buffer_size);
        dhav_packet_offset += buffer_size;
        if(!partial)
        {
          std::cout << "not partial " << buffer_size << " dist " << std::distance(current, last) << std::endl;
          // std::advance(current, buffer_size - std::min<int>(buffer_size, 120));
          dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)24));
          assert(std::distance(std::prev(current, 4), last) >= 4);
          // std::advance(current, buffer_size < 120 ? buffer_size : 120);

          int end_dhav_packet_size = *(std::uint32_t*)(&*current-4);
          // int end_dhav_packet_size = *std::next(current, 0);
          // end_dhav_packet_size += *std::next(current, 1) << 8;
          // end_dhav_packet_size += *std::next(current, 2) << 16;
          // end_dhav_packet_size += *std::next(current, 3) << 24;
          
          assert(end_dhav_packet_size == dhav_packet_size);
          
          std::cout << "reamining bytes in packet from next dhav packet " << std::distance(current, last) << std::endl;
          next_dhav_packet = true;

          finished_dhav();
        }
        // else
        //   std::advance(current, buffer_size);
        // l.lock();
        // packets.emplace_back(std::move(v));
        // l.unlock();
      }
    }
    std::cout << "out of next packet loop " << std::distance(current, last) << std::endl;
  }
  
  /*  void get_buffer(std::string_view buffer)
  {
    std::unique_lock<std::mutex> l(mutex);
    if(!ready)
    {
      ready = true;
      condvar.notify_all();
    }
    l.unlock();

    char dhav[4] = {'D', 'H', 'A', 'V'};
    
    auto current = buffer.begin()
      , last = buffer.end();

    while(current != last)
    {
      std::cout << "loop " << std::distance(current, last) << std::endl;
      if(next_dhav_packet)
      {
      auto begin_buffer = current;
      std::cout << "next_packet" << std::endl;
      // search dhav packet header
      while(std::distance(current, last) > 16
            && (*std::next(current, 0) != dhav[0]
                || *std::next(current, 1) != dhav[1]
                || *std::next(current, 2) != dhav[2]
                || *std::next(current, 3) != dhav[3]))
        ++current;

      //if(std::distance(begin_buffer, current))
      std::cout << "thrown away " << std::distance(begin_buffer, current) << std::endl;

      if(std::distance(current, last) > 16) // found beginning of dhav packet
      {
        dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)128));

        dhav_packet_size = *((std::uint32_t*)((&*current + 12)));
        // dhav_packet_size = *std::next(current, 12);
        // dhav_packet_size += *std::next(current, 13) << 8;
        // dhav_packet_size += *std::next(current, 14) << 16;
        // dhav_packet_size += *std::next(current, 15) << 24;
        dhav_packet_offset = 0u;

        std::advance(current, 16);
          
        std::cout << "dhav size " << dhav_packet_size << " remaining buffer size " << std::distance(current, last) << std::endl;

        bool partial = std::distance(current, last) < dhav_packet_size - 20;
        unsigned int buffer_size = std::min((std::ptrdiff_t)dhav_packet_size - 20, std::distance(current, last));
          
        std::vector<char> v(current, std::next(current, buffer_size));
        current += buffer_size;
        if(!partial)
        {
          std::cout << "not partial" << std::endl;
          dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)128));
          int end_dhav_packet_size = *(std::uint32_t*)&*current;
          std::advance(current, 4);
          assert(end_dhav_packet_size == dhav_packet_size);
        }
        else
        {
          std::cout << "partial" << std::endl;
          next_dhav_packet = false;
        }

        dhav_packet_offset = buffer_size;

        l.lock();
        packets.emplace_back(std::move(v));
        l.unlock();
      }
      }
      else
      {
        std::cout << "middle of DHAV packet offset: " << dhav_packet_offset << " size " << dhav_packet_size << std::endl;
        bool partial = dhav_packet_offset + std::distance(current, last) < dhav_packet_size - 20;
        std::size_t buffer_size = std::min((std::ptrdiff_t)(dhav_packet_size - 20 - dhav_packet_offset)
                                             , std::distance(current, last));
        std::cout << "buffer size " << buffer_size << " partial " << partial << std::endl;
        std::vector<char> v(current, std::next(current, buffer_size));
        std::advance(current, buffer_size);
        if(!partial)
        {
          std::cout << "not partial " << buffer_size << " dist " << std::distance(current, last) << std::endl;
          // std::advance(current, buffer_size - std::min<int>(buffer_size, 120));
          dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)24));
          assert(std::distance(current, last) >= 4);
          // std::advance(current, buffer_size < 120 ? buffer_size : 120);

          int end_dhav_packet_size = *(std::uint32_t*)&*current;
          // int end_dhav_packet_size = *std::next(current, 0);
          // end_dhav_packet_size += *std::next(current, 1) << 8;
          // end_dhav_packet_size += *std::next(current, 2) << 16;
          // end_dhav_packet_size += *std::next(current, 3) << 24;
          
          std::advance(current, 4);
          assert(end_dhav_packet_size == dhav_packet_size);
          
          std::cout << "reamining bytes in packet from next dhav packet " << std::distance(current, last) << std::endl;
          next_dhav_packet = true;
        }
        // else
        //   std::advance(current, buffer_size);
        dhav_packet_offset += buffer_size;
        l.lock();
        packets.emplace_back(std::move(v));
        l.unlock();
      }
    }
    std::cout << "out of next packet loop " << std::distance(current, last) << std::endl;
    dahua_ptc::print_packet(current, std::min(std::distance(current, last), (std::ptrdiff_t)24));
    // else

  }
  */
};

struct GstDmssSrcClass : GstPushSrcClass
{
};

// #define VTS_VIDEO_CAPS                                               \
//   "video/x-h264, stream-format=(string) { byte-stream, avc }, "        \
//   "width = " GST_VIDEO_SIZE_RANGE ", "                                 \
//   "height = " GST_VIDEO_SIZE_RANGE ", "                                \
//   "framerate = " GST_VIDEO_FPS_RANGE

#define VTS_VIDEO_CAPS "video/x-h264"


static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
                                                                   (
    "video/x-h264, parsed = (boolean) true, stream-format=(string) { avc, avc3, byte-stream }, alignment=(string) { au, nal } "
                                                                    )
    );

extern "C" {

G_DEFINE_TYPE (GstDmssSrc, gst_dmsssrc, GST_TYPE_PUSH_SRC);

}

gboolean GstDmssSrc::query (GstBaseSrc * bsrc, GstQuery * query)
{
  assert(bsrc != nullptr);
  std::cout << "query" << std::endl;
  std::cout << "query type " << (int)GST_QUERY_TYPE (query) << std::endl;

  if(GST_QUERY_TYPE(query) == GST_QUERY_CAPS)
  {
    return false;
  }
  
  return GST_BASE_SRC_CLASS(gst_dmsssrc_parent_class)->query(bsrc, query);
  // return false;
}
  
GstCaps* GstDmssSrc::fixate(GstBaseSrc * bsrc, GstCaps* caps)
{
  std::cout << "Fixate" << std::endl;
  GstCaps* c = gst_caps_new_empty();
  c = gst_caps_make_writable (c);
  return GST_BASE_SRC_CLASS(gst_dmsssrc_parent_class)->fixate(bsrc, c);
}

void work(GstDmssSrc* src)
{
  namespace asio = boost::asio;
  
  asio::io_service io_service;
  
  asio::io_service::strand receive_strand(io_service);
  asio::io_service::strand server_strand(io_service);

  auto read_channel = [&] (asio::yield_context yield)
    {
      std::cout << "connecting to " << src->hostname << " with port " << src->port << std::endl;
      dahua_ptc::read_channel({asio::ip::address::from_string(src->hostname), src->port}
                              , receive_strand, src->username, src->password, 1, yield
                              , std::bind(&GstDmssSrc::get_buffer, src, std::placeholders::_1));
    };
  
  asio::spawn(receive_strand, read_channel);

  io_service.run();
}

extern "C" {
  
static void gst_plugin_template_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_plugin_template_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_plugin_template_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDmssSrc *self = reinterpret_cast<GstDmssSrc*> (object);
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;

  switch (prop_id) {
  case PROP_URL:
    {
      const char* url = g_value_get_string (value);
      self->hostname = url;
    }
    break;
  case PROP_USERNAME:
    {
      const char* string = g_value_get_string (value);
      self->username = string;
    }
    break;
  case PROP_PASSWORD:
    {
      const char* string = g_value_get_string (value);
      self->password = string;
    }
    break;
  case PROP_PORT:
    {
      std::cout << "port" << std::endl;
      self->port = g_value_get_int (value);
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_plugin_template_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDmssSrc *filter = reinterpret_cast<GstDmssSrc*> (object);
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;

  // switch (prop_id) {
  //   case PROP_SILENT:
  //     g_value_set_boolean (value, filter->silent);
  //     break;
  //   default:
  //     G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  //     break;
  // }
}

static gboolean
gst_dmsssrc_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  return false;
}

static GstCaps *gst_dmsssrc_src_fixate (GstBaseSrc * bsrc,
    GstCaps * caps)
{
  std::cerr << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  GstDmssSrc* src = (G_TYPE_CHECK_INSTANCE_CAST((bsrc),gst_dmsssrc_get_type(),GstDmssSrc));
  return src->fixate(bsrc, caps);
}

static gboolean gst_dmsssrc_is_seekable (GstBaseSrc * psrc)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  return false;
}

static gboolean gst_dmsssrc_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  return true;
}

static gboolean gst_dmsssrc_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstDmssSrc* src = (G_TYPE_CHECK_INSTANCE_CAST((bsrc),gst_dmsssrc_get_type(),GstDmssSrc));
  assert(src != nullptr);
  return src->query(bsrc, query);
}

static void gst_dmsssrc_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;

  GstClockTime timestamp = GST_BUFFER_PTS (buffer);
  *start = timestamp;
}

static gboolean gst_dmsssrc_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  return GST_BASE_SRC_CLASS (gst_dmsssrc_parent_class)->decide_allocation (bsrc, query);
}
  
static GstFlowReturn gst_dmsssrc_fill (GstPushSrc * psrc,
    GstBuffer * outbuf)
{
  // std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  GstDmssSrc* src = (G_TYPE_CHECK_INSTANCE_CAST((psrc),gst_dmsssrc_get_type(),GstDmssSrc));
  std::unique_lock<std::mutex> lock(src->mutex);

  if(!src->packets.empty())
  {
    std::vector<char>& packet = src->packets.front();

    // if(packet.size() == 163 || packet.size() == 43)
    // {
    //   src->packets.pop_front();
    //   gst_buffer_resize(outbuf, 0, 0);
    //   return GST_FLOW_OK;
    // }

    GstMapInfo map;

    gst_buffer_map (outbuf, &map, GST_MAP_READWRITE);
    // std::cout << "buffer size is " << map.size << std::endl;
    // std::cout << "packet size is " << packet.size() << std::endl;
    std::copy(packet.begin(), packet.begin() + std::min(map.size, packet.size()), map.data);
    
    gst_buffer_unmap (outbuf, &map);

    src->offset += packet.size();

    if(map.size >= packet.size())
    {
      gst_buffer_resize(outbuf, 0, packet.size());
      std::cout << "sending buffer with " << packet.size() << " (resizing buffer)" << std::endl;
      src->packets.pop_front();
    }
    else
    {
      packet.erase(packet.begin(), packet.begin() + map.size);
      std::cout << "sending buffer with " << map.size << " (removing data from queue)" << std::endl;
    }
    lock.unlock();
  }
  else
  {
    std::cout << "empty" << std::endl;
    gst_buffer_resize(outbuf, 0, 0);
  }
  
  return GST_FLOW_OK;
}
  
static gboolean gst_dmsssrc_start (GstBaseSrc * basesrc)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;

  GstDmssSrc* src = (G_TYPE_CHECK_INSTANCE_CAST((basesrc),gst_dmsssrc_get_type(),GstDmssSrc));

  src->work_thread = std::thread{std::bind(&::work, src)};

  std::unique_lock<std::mutex> l(src->mutex);
  while(!src->ready)
  {
    src->condvar.wait(l);
  }

  std::cout << "Returning from start, data is flowing" << std::endl;
  
  return true;
}
  
static gboolean gst_dmsssrc_stop (GstBaseSrc * basesrc)
{
  std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
  return false;
}
  
static void
gst_dmsssrc_class_init (GstDmssSrcClass * klass)
{
  GstPushSrcClass *gstpushsrc_class = klass;
  GstBaseSrcClass *gstbasesrc_class = &gstpushsrc_class->parent_class;
  GstElementClass *gstelement_class = &gstbasesrc_class->parent_class;
  GObjectClass    *gobject_class    = &gstelement_class->parent_class /* GstObjectClass */.parent_class;

  gobject_class->set_property = gst_plugin_template_set_property;
  gobject_class->get_property = gst_plugin_template_get_property;
  // gstbasesrc_class->set_caps = gst_dmsssrc_setcaps;
  // gstbasesrc_class->fixate = gst_dmsssrc_src_fixate;
  // gstbasesrc_class->is_seekable = gst_dmsssrc_is_seekable;
  // gstbasesrc_class->do_seek = gst_dmsssrc_do_seek;
  gstbasesrc_class->query = gst_dmsssrc_query;
  // gstbasesrc_class->get_times = gst_dmsssrc_get_times;
  gstbasesrc_class->start = gst_dmsssrc_start;
  gstbasesrc_class->stop = gst_dmsssrc_stop;
  // gstbasesrc_class->decide_allocation = gst_dmsssrc_decide_allocation;
  gstpushsrc_class->fill = &gst_dmsssrc_fill;

  g_object_class_install_property (gobject_class, PROP_URL,
      g_param_spec_string ("hostname", "hostname", "hostname or IP to camera",
          "", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_USERNAME,
      g_param_spec_string ("username", "username", "username to camera",
          "", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PASSWORD,
      g_param_spec_string ("password", "password", "password to camera",
          "", G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class, PROP_PORT,
                                   g_param_spec_int ("port", "Port", "Port number, default is 37777", 0, 65535, 37777
                                                     , G_PARAM_READWRITE));
  
  gst_element_class_set_details_simple(gstelement_class,
    "DmssSrc",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "Felipe Magno de Almeida <felipe@expertisesolutions.com.br>");
  
  gst_element_class_add_pad_template (gstelement_class,
    gst_static_pad_template_get (&src_factory));

  std::cout << "inited class " << klass << std::endl;
}
  
/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template plugin' with your description
   */
  // GST_DEBUG_CATEGORY_INIT (gst_plugin_template_debug, "plugin",
  //     0, "Template plugin");

  return gst_element_register (plugin, "dmsssrc", GST_RANK_NONE,
                               gst_dmsssrc_get_type());
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_dmsssrc_init (GstDmssSrc * src)
{
  // filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  // gst_pad_set_event_function (filter->sinkpad,
  //                             GST_DEBUG_FUNCPTR(gst_plugin_template_sink_event));
  // gst_pad_set_chain_function (filter->sinkpad,
  //                             GST_DEBUG_FUNCPTR(gst_plugin_template_chain));
  // GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  // gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  // src->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  // GST_PAD_SET_PROXY_CAPS (src->srcpad);
  // gst_element_add_pad (GST_ELEMENT (src), src->srcpad);

  // gst_base_src_set_live (GST_BASE_SRC (static_cast<GstPushSrc*>(src)), TRUE);

  new (src) GstDmssSrc;
  
  std::cout << "init" << std::endl;
}
  
/* gstreamer looks for this structure to register plugins
 *
 * exchange the string 'Template plugin' with your plugin description
 */
#define PACKAGE "dmsssrc"
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    dmsssrc,
    "Plugin plugin",
    plugin_init,
    "1.0",
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
#undef PACKAGE

}

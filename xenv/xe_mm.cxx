#include "xe_mm.h"

#include "xe_interfaces.h"
#include "xe_imgapi.h"

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		struct MediaVideoDesc
		{
			uint width;
			uint height;
			uint presentation;
			uint duration;
			uint scale;
			uint unused_0;
			Object * xdevice;
			MediaVideoDesc(void) {}
			~MediaVideoDesc(void) {}
		};
		struct MediaAudioDesc
		{
			uint format;
			uint channels;
			uint frame_rate;
			MediaAudioDesc(void) {}
			~MediaAudioDesc(void) {}
		};
		struct MediaSubtitlesDesc
		{
			uint scale;
			uint flags;
			MediaSubtitlesDesc(void) {}
			~MediaSubtitlesDesc(void) {}
		};
		struct MediaPacket
		{
			uint64 decode;
			uint64 present;
			uint64 duration;
			SafePointer<DataBlock> data;
			int length;
			bool keyframe;
		};
		struct MediaSubtitlesSample
		{
			string text;
			uint flags;
			uint scale;
			uint presentation;
			uint duration;
		};

		void ConvertToEngine(Video::VideoObjectDesc & dest, const MediaVideoDesc & src) noexcept
		{
			dest.Width = src.width;
			dest.Height = src.height;
			dest.TimeScale = src.scale;
			dest.FramePresentation = src.presentation;
			dest.FrameDuration = src.duration;
			dest.Device = src.xdevice ? GetWrappedDevice(src.xdevice) : 0;
		}
		void ConvertToEngine(Audio::StreamDesc & dest, const MediaAudioDesc & src) noexcept
		{
			dest.Format = static_cast<Audio::SampleFormat>(src.format);
			dest.ChannelCount = src.channels;
			dest.FramesPerSecond = src.frame_rate;
		}
		void ConvertToEngine(Subtitles::SubtitleDesc & dest, const MediaSubtitlesDesc & src) noexcept
		{
			dest.TimeScale = src.scale;
			dest.Flags = src.flags;
		}
		void ConvertToEngine(Media::PacketBuffer & dest, const MediaPacket & src) noexcept
		{
			dest.PacketData = src.data;
			dest.PacketDataActuallyUsed = src.length;
			dest.PacketDecodeTime = src.decode;
			dest.PacketRenderTime = src.present;
			dest.PacketRenderDuration = src.duration;
			dest.PacketIsKey = src.keyframe;
		}
		void ConvertToEngine(Subtitles::SubtitleSample & dest, const MediaSubtitlesSample & src) noexcept
		{
			dest.Text = src.text;
			dest.TimeScale = src.scale;
			dest.TimePresent = src.presentation;
			dest.Duration = src.duration;
			dest.Flags = src.flags;
		}
		void ConvertToXE(MediaVideoDesc & dest, const Video::VideoObjectDesc & src, Object * device) noexcept
		{
			dest.width = src.Width;
			dest.height = src.Height;
			dest.scale = src.TimeScale;
			dest.presentation = src.FramePresentation;
			dest.duration = src.FrameDuration;
			dest.xdevice = device;
		}
		void ConvertToXE(MediaAudioDesc & dest, const Audio::StreamDesc & src) noexcept
		{
			dest.format = uint(src.Format);
			dest.channels = src.ChannelCount;
			dest.frame_rate = src.FramesPerSecond;
		}
		void ConvertToXE(MediaSubtitlesDesc & dest, const Subtitles::SubtitleDesc & src) noexcept
		{
			dest.scale = src.TimeScale;
			dest.flags = src.Flags;
		}
		void ConvertToXE(MediaPacket & dest, const Media::PacketBuffer & src) noexcept
		{
			dest.data = src.PacketData;
			dest.length = src.PacketDataActuallyUsed;
			dest.decode = src.PacketDecodeTime;
			dest.present = src.PacketRenderTime;
			dest.duration = src.PacketRenderDuration;
			dest.keyframe = src.PacketIsKey;
		}
		void ConvertToXE(MediaSubtitlesSample & dest, const Subtitles::SubtitleSample & src) noexcept
		{
			dest.text = src.Text;
			dest.scale = src.TimeScale;
			dest.presentation = src.TimePresent;
			dest.duration = src.Duration;
			dest.flags = src.Flags;
		}

		class XTrackDesc : public DynamicObject
		{
		protected:
			SafePointer<Media::TrackFormatDesc> _desc;
		public:
			virtual uint GetTrackType(void) noexcept = 0;
			virtual string GetCodecName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _desc->GetTrackCodec(); XE_TRY_OUTRO(L"") }
			virtual SafePointer<DataBlock> GetCodecMagic(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto magic = _desc->GetCodecMagic();
				return magic ? new DataBlock(*magic) : 0;
				XE_TRY_OUTRO(0)
			}
			virtual void SetCodecMagic(SafePointer<DataBlock> & magic, ErrorContext & ectx) noexcept { _desc->SetCodecMagic(magic); }
			virtual SafePointer<XTrackDesc> Clone(ErrorContext & ectx) noexcept = 0;
			Media::TrackFormatDesc * GetValue(void) noexcept { return _desc; }
		};
		class XVideoTrackDesc : public XTrackDesc
		{
		public:
			XVideoTrackDesc(Media::VideoTrackFormatDesc * desc) { _desc.SetRetain(desc); }
			virtual ~XVideoTrackDesc(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				auto & name = cls->GetClassName();
				if (name == L"objectum" || name == L"objectum_dynamicum" || name == L"media.descriptio_portionis" || name == L"video.descriptio_portionis") {
					Retain();
					return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual uint GetTrackType(void) noexcept override { return 2; }
			virtual SafePointer<XTrackDesc> Clone(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				SafePointer<Media::VideoTrackFormatDesc> desc = static_cast<Media::VideoTrackFormatDesc *>(_desc.Inner()->Clone());
				desc->SetCodecMagic(_desc->GetCodecMagic());
				return new XVideoTrackDesc(desc);
				XE_TRY_OUTRO(0)
			}
			virtual MediaVideoDesc GetDescription(void) noexcept
			{
				auto sd = static_cast<Media::VideoTrackFormatDesc *>(_desc.Inner());
				MediaVideoDesc desc;
				desc.width = sd->GetWidth();
				desc.height = sd->GetHeight();
				desc.scale = sd->GetFrameRateDenominator();
				desc.presentation = 0;
				desc.duration = sd->GetFrameRateNumerator();
				desc.unused_0 = 0;
				desc.xdevice = 0;
				return desc;
			}
			virtual uint GetPixelWidth(void) noexcept
			{
				auto sd = static_cast<Media::VideoTrackFormatDesc *>(_desc.Inner());
				return sd->GetPixelAspectHorizontal();
			}
			virtual uint GetPixelHeight(void) noexcept
			{
				auto sd = static_cast<Media::VideoTrackFormatDesc *>(_desc.Inner());
				return sd->GetPixelAspectVertical();
			}
		};
		class XAudioTrackDesc : public XTrackDesc
		{
		public:
			XAudioTrackDesc(Media::AudioTrackFormatDesc * desc) { _desc.SetRetain(desc); }
			virtual ~XAudioTrackDesc(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				auto & name = cls->GetClassName();
				if (name == L"objectum" || name == L"objectum_dynamicum" || name == L"media.descriptio_portionis" || name == L"audio.descriptio_portionis") {
					Retain();
					return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual uint GetTrackType(void) noexcept override { return 1; }
			virtual SafePointer<XTrackDesc> Clone(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				SafePointer<Media::AudioTrackFormatDesc> desc = static_cast<Media::AudioTrackFormatDesc *>(_desc.Inner()->Clone());
				desc->SetCodecMagic(_desc->GetCodecMagic());
				return new XAudioTrackDesc(desc);
				XE_TRY_OUTRO(0)
			}
			virtual MediaAudioDesc GetDescription(void) noexcept
			{
				auto sd = static_cast<Media::AudioTrackFormatDesc *>(_desc.Inner());
				MediaAudioDesc desc;
				ConvertToXE(desc, sd->GetStreamDescriptor());
				return desc;
			}
			virtual uint GetChannelLayout(void) noexcept
			{
				auto sd = static_cast<Media::AudioTrackFormatDesc *>(_desc.Inner());
				return sd->GetChannelLayout();
			}
		};
		class XSubtitlesTrackDesc : public XTrackDesc
		{
		public:
			XSubtitlesTrackDesc(Media::SubtitleTrackFormatDesc * desc) { _desc.SetRetain(desc); }
			virtual ~XSubtitlesTrackDesc(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				auto & name = cls->GetClassName();
				if (name == L"objectum" || name == L"objectum_dynamicum" || name == L"media.descriptio_portionis" || name == L"subscriptiones.descriptio_portionis") {
					Retain();
					return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual uint GetTrackType(void) noexcept override { return 3; }
			virtual SafePointer<XTrackDesc> Clone(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				SafePointer<Media::SubtitleTrackFormatDesc> desc = static_cast<Media::SubtitleTrackFormatDesc *>(_desc.Inner()->Clone());
				desc->SetCodecMagic(_desc->GetCodecMagic());
				return new XSubtitlesTrackDesc(desc);
				XE_TRY_OUTRO(0)
			}
			virtual MediaSubtitlesDesc GetDescription(void) noexcept
			{
				auto sd = static_cast<Media::SubtitleTrackFormatDesc *>(_desc.Inner());
				MediaSubtitlesDesc desc;
				desc.scale = sd->GetTimeScale();
				desc.flags = sd->GetFlags();
				return desc;
			}
		};

		class XVideoObject : public Object
		{
		protected:
			SafePointer<Video::IVideoObject> _obj;
			SafePointer<Object> _dev;
		public:
			XVideoObject(void) {}
			XVideoObject(Video::IVideoObject * obj, Object * dev) { _obj.SetRetain(obj); _dev.SetRetain(dev); }
			virtual ~XVideoObject(void) override {}
			virtual MediaVideoDesc GetDescription(void) noexcept { MediaVideoDesc desc; ConvertToXE(desc, _obj->GetObjectDescriptor(), _dev); return desc; }
			virtual Video::IVideoObject * GetInnerObject(void) noexcept { return _obj; }
		};
		class XVideoFrame : public XVideoObject
		{
			Video::IVideoFrame * _frame;
		public:
			XVideoFrame(Video::IVideoFrame * frame, Object * device) : XVideoObject(frame, device), _frame(frame) {}
			virtual ~XVideoFrame(void) override {}
			virtual uint GetPresentationTime(void) noexcept { return _frame->GetObjectDescriptor().FramePresentation; }
			virtual void SetPresentationTime(const uint & value) noexcept { _frame->SetFramePresentation(value); }
			virtual uint GetDuration(void) noexcept { return _frame->GetObjectDescriptor().FrameDuration; }
			virtual void SetDuration(const uint & value) noexcept { _frame->SetFrameDuration(value); }
			virtual uint GetTimeScale(void) noexcept { return _frame->GetObjectDescriptor().TimeScale; }
			virtual void SetTimeScale(const uint & value) noexcept { _frame->SetTimeScale(value); }
			virtual SafePointer<Object> GetData(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _frame->QueryFrame();
				if (!frame) throw OutOfMemoryException();
				return CreateXFrame(frame);
				XE_TRY_OUTRO(0)
			}
			Video::IVideoFrame * GetValue(void) noexcept { return _frame; }
		};
		class XVideoTranslator : public Object
		{
			SafePointer<Video::IVideoFrameBlt> _blt;
		public:
			XVideoTranslator(Video::IVideoFrameBlt * blt) { _blt.SetRetain(blt); }
			virtual ~XVideoTranslator(void) override {}
			virtual bool SetInputO(XVideoObject * object) noexcept { return _blt->SetInputFormat(object->GetInnerObject()); }
			virtual bool SetInputF(Graphics::PixelFormat pxf, Codec::AlphaMode am, const MediaVideoDesc & desc) noexcept
			{
				Video::VideoObjectDesc vod;
				ConvertToEngine(vod, desc);
				return _blt->SetInputFormat(pxf, am, vod);
			}
			virtual bool SetOutputO(XVideoObject * object) noexcept { return _blt->SetOutputFormat(object->GetInnerObject()); }
			virtual bool SetOutputF(Graphics::PixelFormat pxf, Codec::AlphaMode am, const MediaVideoDesc & desc) noexcept
			{
				Video::VideoObjectDesc vod;
				ConvertToEngine(vod, desc);
				return _blt->SetOutputFormat(pxf, am, vod);
			}
			virtual bool Reset(void) noexcept { return _blt->Reset(); }
			virtual bool IsInitialized(void) noexcept { return _blt->IsInitialized(); }
			virtual bool ProcessTexture(XVideoFrame * dest, VisualObject * src, const int * subres) noexcept
			{
				if (!dest || !src) return false;
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				SafePointer<Graphics::ITexture> texture;
				src->ExposeInterface(VisualObjectInterfaceTexture, texture.InnerRef(), ectx);
				if (ectx.error_code) return false;
				return _blt->Process(dest->GetValue(), texture, Graphics::SubresourceIndex(subres[0], subres[1]));
			}
			virtual bool ProcessFrame(VisualObject * dest, XVideoFrame * src, const int * subres) noexcept
			{
				if (!dest || !src) return false;
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				SafePointer<Graphics::ITexture> texture;
				dest->ExposeInterface(VisualObjectInterfaceTexture, texture.InnerRef(), ectx);
				if (ectx.error_code) return false;
				return _blt->Process(texture, src->GetValue(), Graphics::SubresourceIndex(subres[0], subres[1]));
			}
		};
		class XVideoEncodingSession : public XVideoObject
		{
			Video::IVideoDecoder * _dec;
			Video::IVideoEncoder * _enc;
		public:
			XVideoEncodingSession(const string & codec, const MediaVideoDesc & desc, uint numopt, const uint * opt)
			{
				Video::VideoObjectDesc sd;
				ConvertToEngine(sd, desc);
				_enc = Video::CreateEncoder(codec, sd, numopt, opt);
				if (!_enc) throw InvalidArgumentException();
				_obj = _enc;
				if (_obj->GetObjectDescriptor().Device) _dev.SetRetain(desc.xdevice);
			}
			XVideoEncodingSession(XVideoTrackDesc & desc, Object * device)
			{
				_dec = Video::CreateDecoder(*desc.GetValue(), device ? GetWrappedDevice(device) : 0);
				if (!_dec) throw InvalidArgumentException();
				_obj = _dec;
				if (_obj->GetObjectDescriptor().Device) _dev.SetRetain(device);
			}
			virtual ~XVideoEncodingSession(void) override {}
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual bool Reset(void) noexcept { return _dec ? _dec->Reset() : _enc->Reset(); }
			virtual bool IsDataPending(void) noexcept { return _dec ? _dec->IsOutputAvailable() : _enc->IsOutputAvailable(); }
			virtual bool WritePacket(MediaPacket & packet) noexcept
			{
				if (!_dec) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				return _dec->SupplyPacket(buffer);
			}
			virtual bool ReadPacket(MediaPacket & packet) noexcept
			{
				if (!_enc) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				auto status = _enc->ReadPacket(buffer);
				ConvertToXE(packet, buffer);
				return status;
			}
			virtual bool WriteFrame(XVideoFrame * frame, bool keyframe) noexcept
			{
				if (!_enc) return false;
				return _enc->SupplyFrame(frame->GetValue(), keyframe);
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->SupplyEndOfStream();
			}
			virtual bool ReadFrame(SafePointer<XVideoFrame> & frame) noexcept
			{
				if (!_dec) return false;
				SafePointer<Video::IVideoFrame> read;
				auto status = _dec->ReadFrame(read.InnerRef());
				if (read) try { frame = new XVideoFrame(read, read->GetObjectDescriptor().Device ? _dev.Inner() : 0); } catch (...) { return false; }
				return status;
			}
			virtual SafePointer<XVideoTrackDesc> GetTrackDesc(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_enc) throw Exception();
				SafePointer<Media::TrackFormatDesc> desc = _enc->GetEncodedDescriptor().Clone();
				desc->SetCodecMagic(_enc->GetEncodedDescriptor().GetCodecMagic());
				return new XVideoTrackDesc(static_cast<Media::VideoTrackFormatDesc *>(desc.Inner()));
				XE_TRY_OUTRO(0)
			}
		};
		class XVideoStream : public XVideoObject
		{
			Video::VideoDecoderStream * _dec;
			Video::VideoEncoderStream * _enc;
		public:
			XVideoStream(Media::IMediaTrackSource * track, Object * device)
			{
				_dec = new Video::VideoDecoderStream(track, device ? GetWrappedDevice(device) : 0);
				_obj = _dec;
				if (_obj->GetObjectDescriptor().Device) _dev.SetRetain(device);
			}
			XVideoStream(Media::IMediaContainerSource * container, Object * device)
			{
				_dec = new Video::VideoDecoderStream(container, device ? GetWrappedDevice(device) : 0);
				_obj = _dec;
				if (_obj->GetObjectDescriptor().Device) _dev.SetRetain(device);
			}
			XVideoStream(Streaming::Stream * stream, Object * device)
			{
				_dec = new Video::VideoDecoderStream(stream, device ? GetWrappedDevice(device) : 0);
				_obj = _dec;
				if (_obj->GetObjectDescriptor().Device) _dev.SetRetain(device);
			}
			XVideoStream(Media::IMediaContainerSink * container, const string & codec, const MediaVideoDesc & desc, uint num_opt, const uint * opt)
			{
				Video::VideoObjectDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Video::VideoEncoderStream(container, codec, sd, num_opt, opt);
				_obj = _enc;
				if (_obj->GetObjectDescriptor().Device) _dev = desc.xdevice;
			}
			XVideoStream(Streaming::Stream * stream, const string & media, const string & codec, const MediaVideoDesc & desc, uint num_opt, const uint * opt)
			{
				Video::VideoObjectDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Video::VideoEncoderStream(stream, media, codec, sd, num_opt, opt);
				_obj = _enc;
				if (_obj->GetObjectDescriptor().Device) _dev = desc.xdevice;
			}
			virtual ~XVideoStream(void) override {}
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual uint64 GetDuration(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetDuration(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetPosition(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetCurrentTime(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual void SetPosition(const uint64 & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) _dec->SetCurrentTime(value); else throw Exception();
				XE_TRY_OUTRO()
			}
			virtual uint64 GetScale(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetTimeScale(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<Object> GetTrack(ErrorContext & ectx) noexcept;
			virtual bool WriteFrame(XVideoFrame * frame, bool keyframe) noexcept
			{
				if (!_enc) return false;
				return _enc->WriteFrame(frame->GetValue(), keyframe);
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->Finalize();
			}
			virtual bool ReadFrame(SafePointer<XVideoFrame> & frame) noexcept
			{
				if (!_dec) return false;
				SafePointer<Video::IVideoFrame> read;
				auto status = _dec->ReadFrame(read.InnerRef());
				if (read) try { frame = new XVideoFrame(read, read->GetObjectDescriptor().Device ? _dev.Inner() : 0); } catch (...) { return false; }
				return status;
			}
		};
		class XVideoDevice : public XVideoObject
		{
			Video::IVideoDevice * _device;
		public:
			XVideoDevice(Video::IVideoDevice * device, Object * gpdev) : XVideoObject(device, gpdev), _device(device) {}
			virtual ~XVideoDevice(void) override {}
			virtual string GetIdentifier(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _device->GetDeviceIdentifier(); XE_TRY_OUTRO(L"") }
			virtual SafePointer< Array<MediaVideoDesc> > GetFormats(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<Video::VideoObjectDesc> > desc = _device->GetSupportedFrameFormats();
				SafePointer< Array<MediaVideoDesc> > result = new Array<MediaVideoDesc>(desc ? desc->Length() : 1);
				if (desc) for (auto & d : desc->Elements()) { MediaVideoDesc mvd; ConvertToXE(mvd, d, 0); result->Append(mvd); }
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual bool SetFormat(MediaVideoDesc & desc) noexcept
			{
				Video::VideoObjectDesc vod;
				ConvertToEngine(vod, desc);
				return _device->SetFrameFormat(vod);
			}
			virtual bool GetFrameRateRange(uint * num_min, uint * den_min, uint * num_max, uint * den_max) noexcept { return _device->GetSupportedFrameRateRange(num_min, den_min, num_max, den_max); }
			virtual bool SetFrameRate(uint num, uint den) noexcept { return _device->SetFrameRate(num, den); }
			virtual bool Initialize(void) noexcept { return _device->Initialize(); }
			virtual bool Start(void) noexcept { return _device->StartProcessing(); }
			virtual bool Pause(void) noexcept { return _device->PauseProcessing(); }
			virtual bool Stop(void) noexcept { return _device->StopProcessing(); }
			virtual bool ReadFrame(SafePointer<XVideoFrame> * output, bool * status, IDispatchTask * task) noexcept
			{
				try {
					SafePointer<IDispatchTask> inner;
					inner.SetRetain(task);
					auto outer = CreateStructuredTask< SafePointer<Video::IVideoFrame>, bool >([output, status, inner, gp = _dev](SafePointer<Video::IVideoFrame> & f, bool s) {
						if (f) try {
							SafePointer<XVideoFrame> xf = new XVideoFrame(f, f->GetObjectDescriptor().Device ? gp.Inner() : 0);
							if (output) *output = xf;
						} catch (...) {
							if (status) *status = false;
							if (inner) inner->DoTask(0);
							return;
						}
						if (status) *status = s;
						if (inner) inner->DoTask(0);
					});
					return _device->ReadFrameAsync(outer->Value1.InnerRef(), &outer->Value2, outer);
				} catch (...) { return false; }
			}
		};
		class XVideoFactory : public Object
		{
			SafePointer<Video::IVideoFactory> _fact;
		public:
			XVideoFactory(void) { _fact = Video::CreateVideoFactory(); if (!_fact) throw OutOfMemoryException(); }
			virtual ~XVideoFactory(void) override {}
			virtual SafePointer< Array<Volumes::KeyValuePair<string, string> > > GetDevices(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<Volumes::KeyValuePair<string, string> > > result = new Array<Volumes::KeyValuePair<string, string> >(0x20);
				SafePointer< Volumes::Dictionary<string, string> > dict = _fact->GetAvailableDevices();
				if (dict) for (auto & v : dict->Elements()) result->Append(v);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XVideoDevice> CreateDevice(const string & name, Object * gpdev) noexcept
			{
				try {
					SafePointer<Video::IVideoDevice> dev = _fact->CreateDevice(name, gpdev ? GetWrappedDevice(gpdev) : 0);
					return dev ? new XVideoDevice(dev, gpdev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XVideoDevice> CreateDefaultDevice(Object * gpdev) noexcept
			{
				try {
					SafePointer<Video::IVideoDevice> dev = _fact->CreateDefaultDevice(gpdev ? GetWrappedDevice(gpdev) : 0);
					return dev ? new XVideoDevice(dev, gpdev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XVideoDevice> CreateScreenDevice(VisualObject * screen, Object * gpdev) noexcept
			{
				try {
					if (!screen) return 0;
					ErrorContext ectx;
					ectx.error_code = ectx.error_subcode = 0;
					SafePointer<Windows::IScreen> sobj;
					screen->ExposeInterface(VisualObjectInterfaceScreen, sobj.InnerRef(), ectx);
					if (ectx.error_code) return 0;
					SafePointer<Video::IVideoDevice> dev = _fact->CreateScreenCaptureDevice(sobj, gpdev ? GetWrappedDevice(gpdev) : 0);
					return dev ? new XVideoDevice(dev, gpdev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XVideoFrame> CreateFrameA(handle xframe, Object * gpdev) noexcept
			{
				SafePointer<Codec::Frame> cframe = ExtractFrameFromXFrame(xframe);
				SafePointer<Video::IVideoFrame> frame = _fact->CreateFrame(cframe, gpdev ? GetWrappedDevice(gpdev) : 0);
				if (!frame) return 0;
				try { return new XVideoFrame(frame, frame->GetObjectDescriptor().Device ? gpdev : 0); } catch (...) { return 0; }
			}
			virtual SafePointer<XVideoFrame> CreateFrameB(uint pxf, uint af, const MediaVideoDesc & desc) noexcept
			{
				Codec::AlphaMode am;
				Video::VideoObjectDesc vod;
				Graphics::PixelFormat pm = static_cast<Graphics::PixelFormat>(pxf);
				if (af == 1) am == Codec::AlphaMode::Premultiplied; else am = Codec::AlphaMode::Straight;
				ConvertToEngine(vod, desc);
				SafePointer<Video::IVideoFrame> frame = _fact->CreateFrame(pm, am, vod);
				if (!frame) return 0;
				try { return new XVideoFrame(frame, frame->GetObjectDescriptor().Device ? desc.xdevice : 0); } catch (...) { return 0; }
			}
			virtual SafePointer<XVideoTranslator> CreateTranslator(void) noexcept
			{
				try {
					SafePointer<Video::IVideoFrameBlt> blt = _fact->CreateFrameBlt();
					return blt ? new XVideoTranslator(blt) : 0;
				} catch (...) { return 0; }
			}
		};

		class XWave : public Object
		{
			SafePointer<Audio::WaveBuffer> _buffer;
		public:
			XWave(const MediaAudioDesc & desc, uint64 length) { Audio::StreamDesc sd; ConvertToEngine(sd, desc); _buffer = new Audio::WaveBuffer(sd, length); }
			XWave(Audio::WaveBuffer * buffer) { _buffer.SetRetain(buffer); }
			virtual ~XWave(void) override {}
			virtual SafePointer<XWave> Clone(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> copy = new Audio::WaveBuffer(_buffer);
				return new XWave(copy);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XWave> ConvertFormatE(Audio::SampleFormat format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> conv = _buffer->ConvertFormat(format);
				return new XWave(conv);
				XE_TRY_OUTRO(0)
			}
			virtual void ConvertFormatI(Audio::SampleFormat format, XWave * dest, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_buffer->ConvertFormat(dest->GetValue(), format);
				XE_TRY_OUTRO()
			}
			virtual SafePointer<XWave> ConvertFrameRateE(uint rate, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> conv = _buffer->ConvertFrameRate(rate);
				return new XWave(conv);
				XE_TRY_OUTRO(0)
			}
			virtual void ConvertFrameRateI(uint rate, XWave * dest, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_buffer->ConvertFrameRate(dest->GetValue(), rate);
				XE_TRY_OUTRO()
			}
			virtual SafePointer<XWave> RemuxChannelsE(uint number, const double * matrix, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> conv = _buffer->RemuxChannels(number, matrix);
				return new XWave(conv);
				XE_TRY_OUTRO(0)
			}
			virtual void RemuxChannelsI(uint number, const double * matrix, XWave * dest, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_buffer->RemuxChannels(dest->GetValue(), number, matrix);
				XE_TRY_OUTRO()
			}
			virtual SafePointer<XWave> ReorderChannelsE(uint number, const uint * index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> conv = _buffer->ReorderChannels(number, index);
				return new XWave(conv);
				XE_TRY_OUTRO(0)
			}
			virtual void ReorderChannelsI(uint number, const uint * index, XWave * dest, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_buffer->ReorderChannels(dest->GetValue(), number, index);
				XE_TRY_OUTRO()
			}
			virtual SafePointer<XWave> ReallocateChannelsE(uint number, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Audio::WaveBuffer> conv = _buffer->ReallocateChannels(number);
				return new XWave(conv);
				XE_TRY_OUTRO(0)
			}
			virtual void ReallocateChannelsI(uint number, XWave * dest, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_buffer->ReallocateChannels(dest->GetValue(), number);
				XE_TRY_OUTRO()
			}
			virtual MediaAudioDesc GetDescription(void) noexcept
			{
				MediaAudioDesc desc;
				ConvertToXE(desc, _buffer->GetFormatDescriptor());
				return desc;
			}
			virtual uint64 GetNumberOfFrames(void) noexcept { return _buffer->GetSizeInFrames(); }
			virtual uint64 GetNumberOfFramesUsed(void) noexcept { return _buffer->FramesUsed(); }
			virtual void SetNumberOfFramesUsed(const uint64 & value) noexcept { _buffer->FramesUsed() = value; }
			virtual uint64 GetLength(void) noexcept { return _buffer->GetAllocatedSizeInBytes(); }
			virtual uint64 GetLengthUsed(void) noexcept { return _buffer->GetUsedSizeInBytes(); }
			virtual void * GetData(void) noexcept { return _buffer->GetData(); }
			virtual uint GetFrameRate(void) noexcept { return _buffer->GetFormatDescriptor().FramesPerSecond; }
			virtual void SetFrameRate(const uint & value) noexcept { _buffer->ReinterpretFrames(value); }
			virtual int ReadSampleI(uint64 index, uint channel) noexcept { return _buffer->ReadSampleInteger(index, channel); }
			virtual double ReadSampleF(uint64 index, uint channel) noexcept { return _buffer->ReadSampleFloat(index, channel); }
			virtual void WriteSampleI(uint64 index, uint channel, int value) noexcept { _buffer->WriteSample(index, channel, value); }
			virtual void WriteSampleF(uint64 index, uint channel, double value) noexcept { _buffer->WriteSample(index, channel, value); }
			Audio::WaveBuffer * GetValue(void) noexcept { return _buffer; }
		};
		class XAudioObject : public Object
		{
		public:
			virtual MediaAudioDesc GetDescription(void) noexcept = 0;
			virtual uint GetChannelLayout(void) noexcept = 0;
		};
		class XAudioEncodingSession : public XAudioObject
		{
			SafePointer<Audio::IAudioDecoder> _dec;
			SafePointer<Audio::IAudioEncoder> _enc;
		public:
			XAudioEncodingSession(const string & codec, const MediaAudioDesc & desc, uint numopt, const uint * opt)
			{
				Audio::StreamDesc sd;
				ConvertToEngine(sd, desc);
				_enc = Audio::CreateEncoder(codec, sd, numopt, opt);
				if (!_enc) throw InvalidArgumentException();
			}
			XAudioEncodingSession(XAudioTrackDesc & desc, const MediaAudioDesc * desired)
			{
				Audio::StreamDesc sd;
				if (desired) ConvertToEngine(sd, *desired);
				_dec = Audio::CreateDecoder(*desc.GetValue(), desired ? &sd : 0);
				if (!_dec) throw InvalidArgumentException();
			}
			virtual ~XAudioEncodingSession(void) override {}
			virtual MediaAudioDesc GetDescription(void) noexcept override
			{
				MediaAudioDesc desc;
				ConvertToXE(desc, _dec ? _dec->GetFormatDescriptor() : _enc->GetFormatDescriptor());
				return desc;
			}
			virtual uint GetChannelLayout(void) noexcept override { return _dec ? _dec->GetChannelLayout() : _enc->GetChannelLayout(); }
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual MediaAudioDesc GetNativeDescription(void) noexcept
			{
				MediaAudioDesc desc;
				ConvertToXE(desc, _dec ? _dec->GetEncodedDescriptor() : _enc->GetEncodedDescriptor());
				return desc;
			}
			virtual bool Reset(void) noexcept { return _dec ? _dec->Reset() : _enc->Reset(); }
			virtual int GetPendingPackets(void) noexcept { return _dec ? _dec->GetPendingPacketsCount() : _enc->GetPendingPacketsCount(); }
			virtual int GetPendingFrames(void) noexcept { return _dec ? _dec->GetPendingFramesCount() : _enc->GetPendingFramesCount(); }
			virtual bool WritePacket(MediaPacket & packet) noexcept
			{
				if (!_dec) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				return _dec->SupplyPacket(buffer);
			}
			virtual bool ReadPacket(MediaPacket & packet) noexcept
			{
				if (!_enc) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				auto status = _enc->ReadPacket(buffer);
				ConvertToXE(packet, buffer);
				return status;
			}
			virtual bool WriteFrame(XWave * wave) noexcept
			{
				if (!_enc) return false;
				return _enc->SupplyFrames(wave->GetValue());
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->SupplyEndOfStream();
			}
			virtual bool ReadFrame(XWave * wave) noexcept
			{
				if (!_dec) return false;
				return _dec->ReadFrames(wave->GetValue());
			}
			virtual SafePointer<XAudioTrackDesc> GetTrackDesc(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_enc) throw Exception();
				SafePointer<Media::TrackFormatDesc> desc = _enc->GetFullEncodedDescriptor().Clone();
				desc->SetCodecMagic(_enc->GetFullEncodedDescriptor().GetCodecMagic());
				return new XAudioTrackDesc(static_cast<Media::AudioTrackFormatDesc *>(desc.Inner()));
				XE_TRY_OUTRO(0)
			}
		};
		class XAudioStream : public XAudioObject
		{
			SafePointer<Audio::AudioDecoderStream> _dec;
			SafePointer<Audio::AudioEncoderStream> _enc;
		public:
			XAudioStream(Media::IMediaTrackSource * track, const MediaAudioDesc * desired)
			{
				Audio::StreamDesc sd;
				if (desired) ConvertToEngine(sd, *desired);
				_dec = new Audio::AudioDecoderStream(track, desired ? &sd : 0);
			}
			XAudioStream(Media::IMediaContainerSource * container, const MediaAudioDesc * desired)
			{
				Audio::StreamDesc sd;
				if (desired) ConvertToEngine(sd, *desired);
				_dec = new Audio::AudioDecoderStream(container, desired ? &sd : 0);
			}
			XAudioStream(Streaming::Stream * stream, const MediaAudioDesc * desired)
			{
				Audio::StreamDesc sd;
				if (desired) ConvertToEngine(sd, *desired);
				_dec = new Audio::AudioDecoderStream(stream, desired ? &sd : 0);
			}
			XAudioStream(Media::IMediaContainerSink * container, const string & codec, const MediaAudioDesc & desc, uint num_opt, const uint * opt)
			{
				Audio::StreamDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Audio::AudioEncoderStream(container, codec, sd, num_opt, opt);
			}
			XAudioStream(Streaming::Stream * stream, const string & media, const string & codec, const MediaAudioDesc & desc, uint num_opt, const uint * opt)
			{
				Audio::StreamDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Audio::AudioEncoderStream(stream, media, codec, sd, num_opt, opt);
			}
			virtual ~XAudioStream(void) override {}
			virtual MediaAudioDesc GetDescription(void) noexcept override
			{
				MediaAudioDesc desc;
				ConvertToXE(desc, _dec ? _dec->GetFormatDescriptor() : _enc->GetFormatDescriptor());
				return desc;
			}
			virtual uint GetChannelLayout(void) noexcept override { return _dec ? _dec->GetChannelLayout() : _enc->GetChannelLayout(); }
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual MediaAudioDesc GetNativeDescription(void) noexcept
			{
				MediaAudioDesc desc;
				ConvertToXE(desc, _dec ? _dec->GetEncodedDescriptor() : _enc->GetEncodedDescriptor());
				return desc;
			}
			virtual uint64 GetDuration(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetFramesCount(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetPosition(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetCurrentFrame(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual void SetPosition(const uint64 & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) _dec->SetCurrentFrame(value); else throw Exception();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<Object> GetTrack(ErrorContext & ectx) noexcept;
			virtual bool WriteFrame(XWave * wave) noexcept
			{
				if (!_enc) return false;
				return _enc->WriteFrames(wave->GetValue());
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->Finalize();
			}
			virtual bool ReadFrame(XWave * wave) noexcept
			{
				if (!_dec) return false;
				return _dec->ReadFrames(wave->GetValue());
			}
		};
		class XAudioDevice : public XAudioObject
		{
			SafePointer<Audio::IAudioOutputDevice> _out;
			SafePointer<Audio::IAudioInputDevice> _in;
		public:
			XAudioDevice(Audio::IAudioOutputDevice * out) { _out.SetRetain(out); }
			XAudioDevice(Audio::IAudioInputDevice * in) { _in.SetRetain(in); }
			virtual ~XAudioDevice(void) override {}
			virtual MediaAudioDesc GetDescription(void) noexcept override { MediaAudioDesc desc; ConvertToXE(desc, _out ? _out->GetFormatDescriptor() : _in->GetFormatDescriptor()); return desc; }
			virtual uint GetChannelLayout(void) noexcept override { return _out ? _out->GetChannelLayout() : _in->GetChannelLayout(); }
			virtual string GetIdentifier(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _out ? _out->GetDeviceIdentifier() : _in->GetDeviceIdentifier(); XE_TRY_OUTRO(L"") }
			virtual double GetVolume(void) noexcept { return _out ? _out->GetVolume() : _in->GetVolume(); }
			virtual void SetVolume(const double & value) noexcept { if (_out) _out->SetVolume(value); else _in->SetVolume(value); }
			virtual bool Start(void) noexcept { return _out ? _out->StartProcessing() : _in->StartProcessing(); }
			virtual bool Pause(void) noexcept { return _out ? _out->PauseProcessing() : _in->PauseProcessing(); }
			virtual bool Stop(void) noexcept { return _out ? _out->StopProcessing() : _in->StopProcessing(); }
			virtual bool Schedule(XWave * wave, bool * status, IDispatchTask * task) noexcept { return _out ? _out->WriteFramesAsync(wave->GetValue(), status, task) : _in->ReadFramesAsync(wave->GetValue(), status, task); }
		};
		class XAudioFactory : public Object
		{
			SafePointer<Audio::IAudioDeviceFactory> _fact;
		public:
			XAudioFactory(void) { _fact = Audio::CreateAudioDeviceFactory(); if (!_fact) throw OutOfMemoryException(); }
			virtual ~XAudioFactory(void) override {}
			virtual SafePointer< Array<Volumes::KeyValuePair<string, string> > > GetExDevices(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<Volumes::KeyValuePair<string, string> > > result = new Array<Volumes::KeyValuePair<string, string> >(0x20);
				SafePointer< Volumes::Dictionary<string, string> > dict = _fact->GetAvailableOutputDevices();
				if (dict) for (auto & v : dict->Elements()) result->Append(v);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< Array<Volumes::KeyValuePair<string, string> > > GetInDevices(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<Volumes::KeyValuePair<string, string> > > result = new Array<Volumes::KeyValuePair<string, string> >(0x20);
				SafePointer< Volumes::Dictionary<string, string> > dict = _fact->GetAvailableInputDevices();
				if (dict) for (auto & v : dict->Elements()) result->Append(v);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XAudioDevice> CreateOutputDevice(const string & name) noexcept
			{
				try {
					SafePointer<Audio::IAudioOutputDevice> dev = _fact->CreateOutputDevice(name);
					return dev ? new XAudioDevice(dev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XAudioDevice> CreateDefaultOutputDevice(void) noexcept
			{
				try {
					SafePointer<Audio::IAudioOutputDevice> dev = _fact->CreateDefaultOutputDevice();
					return dev ? new XAudioDevice(dev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XAudioDevice> CreateInputDevice(const string & name) noexcept
			{
				try {
					SafePointer<Audio::IAudioInputDevice> dev = _fact->CreateInputDevice(name);
					return dev ? new XAudioDevice(dev) : 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XAudioDevice> CreateDefaultInputDevice(void) noexcept
			{
				try {
					SafePointer<Audio::IAudioInputDevice> dev = _fact->CreateDefaultInputDevice();
					return dev ? new XAudioDevice(dev) : 0;
				} catch (...) { return 0; }
			}
			virtual bool RegisterEventHandler(Audio::IAudioEventCallback * callback) noexcept { return _fact->RegisterEventCallback(callback); }
			virtual bool UnregisterEventHandler(Audio::IAudioEventCallback * callback) noexcept { return _fact->UnregisterEventCallback(callback); }
		};

		class XSubtitlesEncodingSession : public Object
		{
			SafePointer<Subtitles::ISubtitleDecoder> _dec;
			SafePointer<Subtitles::ISubtitleEncoder> _enc;
		public:
			XSubtitlesEncodingSession(const string & codec, const MediaSubtitlesDesc & desc)
			{
				Subtitles::SubtitleDesc sd;
				ConvertToEngine(sd, desc);
				_enc = Subtitles::CreateEncoder(codec, sd);
				if (!_enc) throw InvalidArgumentException();
			}
			XSubtitlesEncodingSession(XSubtitlesTrackDesc & desc)
			{
				_dec = Subtitles::CreateDecoder(*desc.GetValue());
				if (!_dec) throw InvalidArgumentException();
			}
			virtual ~XSubtitlesEncodingSession(void) override {}
			virtual MediaSubtitlesDesc GetDescription(void) noexcept
			{
				MediaSubtitlesDesc desc;
				auto & sd = _dec ? _dec->GetObjectDescriptor() : _enc->GetObjectDescriptor();
				ConvertToXE(desc, sd);
				return desc;
			}
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual bool Reset(void) noexcept { return _dec ? _dec->Reset() : _enc->Reset(); }
			virtual int GetPendingPackets(void) noexcept { return _dec ? _dec->GetPendingPacketsCount() : _enc->GetPendingPacketsCount(); }
			virtual int GetPendingSamples(void) noexcept { return _dec ? _dec->GetPendingSamplesCount() : _enc->GetPendingSamplesCount(); }
			virtual bool WritePacket(MediaPacket & packet) noexcept
			{
				if (!_dec) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				return _dec->SupplyPacket(buffer);
			}
			virtual bool ReadPacket(MediaPacket & packet) noexcept
			{
				if (!_enc) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				auto status = _enc->ReadPacket(buffer);
				ConvertToXE(packet, buffer);
				return status;
			}
			virtual bool WriteSample(MediaSubtitlesSample & sam) noexcept
			{
				if (!_enc) return false;
				try {
					Subtitles::SubtitleSample ss;
					ConvertToEngine(ss, sam);
					return _enc->SupplySample(ss);
				} catch (...) { return false; }
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->SupplyEndOfStream();
			}
			virtual bool ReadSample(MediaSubtitlesSample & sam) noexcept
			{
				if (!_dec) return false;
				try {
					Subtitles::SubtitleSample ss;
					auto status = _dec->ReadSample(ss);
					ConvertToXE(sam, ss);
					return status;
				} catch (...) { return false; }
			}
			virtual SafePointer<XSubtitlesTrackDesc> GetTrackDesc(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_enc) throw Exception();
				SafePointer<Media::TrackFormatDesc> desc = _enc->GetEncodedDescriptor().Clone();
				desc->SetCodecMagic(_enc->GetEncodedDescriptor().GetCodecMagic());
				return new XSubtitlesTrackDesc(static_cast<Media::SubtitleTrackFormatDesc *>(desc.Inner()));
				XE_TRY_OUTRO(0)
			}
		};
		class XSubtitlesStream : public Object
		{
			SafePointer<Subtitles::SubtitleDecoderStream> _dec;
			SafePointer<Subtitles::SubtitleEncoderStream> _enc;
		public:
			XSubtitlesStream(Media::IMediaTrackSource * track) { _dec = new Subtitles::SubtitleDecoderStream(track); }
			XSubtitlesStream(Media::IMediaContainerSource * container) { _dec = new Subtitles::SubtitleDecoderStream(container); }
			XSubtitlesStream(Streaming::Stream * stream) { _dec = new Subtitles::SubtitleDecoderStream(stream); }
			XSubtitlesStream(Media::IMediaContainerSink * container, const string & codec, const MediaSubtitlesDesc & desc)
			{
				Subtitles::SubtitleDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Subtitles::SubtitleEncoderStream(container, codec, sd);
			}
			XSubtitlesStream(Streaming::Stream * stream, const string & media, const string & codec, const MediaSubtitlesDesc & desc)
			{
				Subtitles::SubtitleDesc sd;
				ConvertToEngine(sd, desc);
				_enc = new Subtitles::SubtitleEncoderStream(stream, media, codec, sd);
			}
			virtual ~XSubtitlesStream(void) override {}
			virtual MediaSubtitlesDesc GetDescription(void) noexcept
			{
				MediaSubtitlesDesc desc;
				auto & sd = _dec ? _dec->GetObjectDescriptor() : _enc->GetObjectDescriptor();
				ConvertToXE(desc, sd);
				return desc;
			}
			virtual string GetCodecName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _dec ? _dec->GetEncodedFormat() : _enc->GetEncodedFormat();
				XE_TRY_OUTRO(L"");
			}
			virtual uint64 GetDuration(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetDuration(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetPosition(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) return _dec->GetCurrentTime(); else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual void SetPosition(const uint64 & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_dec) _dec->SetCurrentTime(value); else throw Exception();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<Object> GetTrack(ErrorContext & ectx) noexcept;
			virtual bool WriteSample(MediaSubtitlesSample & sam) noexcept
			{
				if (!_enc) return false;
				try {
					Subtitles::SubtitleSample ss;
					ConvertToEngine(ss, sam);
					return _enc->WriteFrames(ss);
				} catch (...) { return false; }
			}
			virtual bool WriteEOS(void) noexcept
			{
				if (!_enc) return false;
				return _enc->Finalize();
			}
			virtual bool ReadSample(MediaSubtitlesSample & sam) noexcept
			{
				if (!_dec) return false;
				try {
					Subtitles::SubtitleSample ss;
					auto status = _dec->ReadSample(ss);
					ConvertToXE(sam, ss);
					return status;
				} catch (...) { return false; }
			}
		};

		class XMetadata : public Object
		{
			SafePointer<Media::Metadata> _meta;
		public:
			XMetadata(void) { _meta = new Media::Metadata; }
			XMetadata(Media::Metadata * inner) { if (inner) _meta.SetRetain(inner); else _meta = new Media::Metadata; }
			virtual ~XMetadata(void) override {}
			virtual SafePointer< Array<uint> > GetKeys(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<uint> > result = new Array<uint>(0x20);
				for (auto & m : _meta->Elements()) result->Append(uint(m.key));
				return result;
				XE_TRY_OUTRO(0);
			}
			virtual string GetString(Media::MetadataKey key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value = _meta->GetElementByKey(key);
				if (!value) return L"";
				return value->Text;
				XE_TRY_OUTRO(L"")
			}
			virtual uint GetInteger(Media::MetadataKey key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value = _meta->GetElementByKey(key);
				if (!value) return 0;
				return value->Number;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<DataBlock> GetImage(Media::MetadataKey key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto value = _meta->GetElementByKey(key);
				if (!value) return 0;
				return value->Picture;
				XE_TRY_OUTRO(0)
			}
			virtual void SetString(Media::MetadataKey key, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_meta->Update(key, Media::MetadataValue(value));
				XE_TRY_OUTRO()
			}
			virtual void SetInteger(Media::MetadataKey key, uint value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_meta->Update(key, Media::MetadataValue(value));
				XE_TRY_OUTRO()
			}
			virtual void SetImage(Media::MetadataKey key, DataBlock * data, const string & format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_meta->Update(key, Media::MetadataValue(data, format));
				XE_TRY_OUTRO()
			}
			virtual void Remove(Media::MetadataKey key) noexcept { _meta->Remove(key); }
			virtual void Clear(void) noexcept { _meta->Clear(); }
			virtual SafePointer<XMetadata> Clone(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Media::Metadata> copy = Media::CloneMetadata(_meta);
				return new XMetadata(copy);
				XE_TRY_OUTRO(0);
			}
			Media::Metadata * GetValue(void) noexcept { return _meta; }
		};
		class XTrack : public Object
		{
			SafePointer<Media::IMediaTrackSource> _source;
			SafePointer<Media::IMediaTrackSink> _sink;
		public:
			XTrack(Media::IMediaTrackSource * inner) { _source.SetRetain(inner); }
			XTrack(Media::IMediaTrackSink * inner) { _sink.SetRetain(inner); }
			virtual ~XTrack(void) override {}
			virtual uint GetTrackType(void) noexcept
			{
				auto cls = _source ? _source->GetTrackClass() : _sink->GetTrackClass();
				if (cls == Media::TrackClass::Audio) return 1;
				else if (cls == Media::TrackClass::Video) return 2;
				else if (cls == Media::TrackClass::Subtitles) return 3;
				else return 0;
			}
			virtual SafePointer<XTrackDesc> GetTrackDesc(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto & desc = _source ? _source->GetFormatDescriptor() : _sink->GetFormatDescriptor();
				SafePointer<Media::TrackFormatDesc> copy = desc.Clone();
				copy->SetCodecMagic(desc.GetCodecMagic());
				if (copy->GetTrackClass() == Media::TrackClass::Audio) return new XAudioTrackDesc(static_cast<Media::AudioTrackFormatDesc *>(copy.Inner()));
				else if (copy->GetTrackClass() == Media::TrackClass::Video) return new XVideoTrackDesc(static_cast<Media::VideoTrackFormatDesc *>(copy.Inner()));
				else if (copy->GetTrackClass() == Media::TrackClass::Subtitles) return new XSubtitlesTrackDesc(static_cast<Media::SubtitleTrackFormatDesc *>(copy.Inner()));
				else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual string GetTrackName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _source ? _source->GetTrackName() : _sink->GetTrackName();
				XE_TRY_OUTRO(L"");
			}
			virtual void SetTrackName(const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->SetTrackName(value); else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual string GetTrackLanguage(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return _source ? _source->GetTrackLanguage() : _sink->GetTrackLanguage();
				XE_TRY_OUTRO(L"");
			}
			virtual void SetTrackLanguage(const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->SetTrackLanguage(value); else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual bool GetTrackVisibility(void) noexcept { return _source ? _source->IsTrackVisible() : _sink->IsTrackVisible(); }
			virtual void SetTrackVisibility(const bool & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->MakeTrackVisible(value); else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual bool GetTrackAutoselectable(void) noexcept { return _source ? _source->IsTrackAutoselectable() : _sink->IsTrackAutoselectable(); }
			virtual void SetTrackAutoselectable(const bool & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->MakeTrackAutoselectable(value); else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual uint GetTrackGroup(void) noexcept { return _source ? _source->GetTrackGroup() : _sink->GetTrackGroup(); }
			virtual void SetTrackGroup(const uint & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->SetTrackGroup(value); else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual uint64 GetScale(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetTimeScale();
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual uint64 GetDuration(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetDuration();
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual uint64 GetPosition(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetPosition();
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual void SetPosition(const uint64 & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) _source->Seek(value);
				else throw Exception();
				XE_TRY_OUTRO();
			}
			virtual uint64 GetCurrentPacket(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetCurrentPacket();
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual uint64 GetNumberOfPackets(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetPacketCount();
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual bool ReadPacket(MediaPacket & packet) noexcept
			{
				if (!_source) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				auto status = _source->ReadPacket(buffer);
				ConvertToXE(packet, buffer);
				return status;
			}
			virtual bool WritePacket(MediaPacket & packet) noexcept
			{
				if (!_sink) return false;
				Media::PacketBuffer buffer;
				ConvertToEngine(buffer, packet);
				return _sink->WritePacket(buffer);
			}
			virtual bool WriteCodecMagic(DataBlock * data) noexcept { if (!_sink) return false; return _sink->UpdateCodecMagic(data); }
			virtual bool Finalize(void) noexcept { if (!_sink) return false; return _sink->Finalize(); }
			virtual bool IsFinalized(void) noexcept { if (!_sink) return true; return _sink->Sealed(); }
			Media::IMediaTrackSource * GetSource(void) noexcept { return _source; }
			Media::IMediaTrackSink * GetSink(void) noexcept { return _sink; }
		};
		class XContainer : public Object
		{
			SafePointer<Media::IMediaContainerSource> _source;
			SafePointer<Media::IMediaContainerSink> _sink;
		public:
			XContainer(Media::IMediaContainerSource * inner) { _source.SetRetain(inner); }
			XContainer(Media::IMediaContainerSink * inner) { _sink.SetRetain(inner); }
			virtual ~XContainer(void) override {}
			virtual SafePointer<XMetadata> GetMetadata(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Media::Metadata> mdt;
				if (_source) mdt = _source->ReadMetadata();
				else mdt = _sink->ReadMetadata();
				return new XMetadata(mdt);
				XE_TRY_OUTRO(0)
			}
			virtual int GetTrackCount(void) noexcept
			{
				if (_source) return _source->GetTrackCount();
				else return _sink->GetTrackCount();
			}
			virtual SafePointer<XTrack> GetTrack(int index, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) {
					SafePointer<Media::IMediaTrackSource> track = _source->OpenTrack(index);
					if (!track) throw InvalidArgumentException();
					return new XTrack(track);
				} else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uint64 GetDuration(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_source) return _source->GetDuration();
				else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual bool RewriteMetadataA(XMetadata * meta) noexcept
			{
				if (_source) return _source->PatchMetadata(meta->GetValue());
				else { _sink->WriteMetadata(meta->GetValue()); return true; }
			}
			virtual bool RewriteMetadataB(XMetadata * meta, XStream * dest) noexcept
			{
				try {
					if (_source) {
						SafePointer<Streaming::Stream> stream = WrapFromXStream(dest);
						return _source->PatchMetadata(meta->GetValue(), stream);
					} else return false;
				} catch (...) { return false; }
			}
			virtual SafePointer<XTrack> CreateTrack(XTrackDesc * desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_sink) throw Exception();
				if (!desc) throw InvalidArgumentException();
				SafePointer<Media::IMediaTrackSink> track = _sink->CreateTrack(*desc->GetValue());
				if (!track) throw InvalidStateException();
				return new XTrack(track);
				XE_TRY_OUTRO(0)
			}
			virtual bool Finalize(void) noexcept { if (_sink) return _sink->Finalize(); else return false; }
			virtual bool GetSealAutomatically(void) noexcept { if (_sink) return _sink->IsAutofinalizable(); else return false; }
			virtual void SetSealAutomatically(const bool & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_sink) _sink->SetAutofinalize(value);
				else throw Exception();
				XE_TRY_OUTRO()
			}
			Media::IMediaContainerSource * GetSource(void) noexcept { return _source; }
			Media::IMediaContainerSink * GetSink(void) noexcept { return _sink; }
		};
		class MultimediaExtension : public IAPIExtension
		{
			static SafePointer<XMetadata> _create_metadata(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XMetadata(); XE_TRY_OUTRO(0) }
			static SafePointer<XContainer> _open_container(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				SafePointer<Media::IMediaContainerSource> media = Media::OpenContainer(stream);
				if (!media) throw InvalidFormatException();
				return new XContainer(media);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XContainer> _create_container(XStream * xstream, const string & format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				SafePointer<Media::IMediaContainerSink> media = Media::CreateContainer(stream, format);
				if (!media) throw InvalidFormatException();
				return new XContainer(media);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoTrackDesc> _create_vi_desc(const string & codec, const MediaVideoDesc & desc, uint pxw, uint pxh, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Media::VideoTrackFormatDesc> vsd = new Media::VideoTrackFormatDesc(codec, desc.width, desc.height, desc.duration, desc.scale, pxw, pxh);
				return new XVideoTrackDesc(vsd);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoEncodingSession> _create_vi_dec_session(XVideoTrackDesc & desc, Object * device, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XVideoEncodingSession(desc, device);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoEncodingSession> _create_vi_enc_session(const string & codec, const MediaVideoDesc & desc, uint numopt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XVideoEncodingSession(codec, desc, numopt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoStream> _create_vi_dec_stream_t(XTrack * track, Object * device, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = track->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XVideoStream(src, device);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoStream> _create_vi_dec_stream_c(XContainer * container, Object * device, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XVideoStream(src, device);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoStream> _create_vi_dec_stream_s(XStream * xstream, Object * device, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XVideoStream(stream, device);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoStream> _create_vi_enc_stream_c(XContainer * container, const string & codec, const MediaVideoDesc & desc, uint num_opt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSink();
				if (!src) throw InvalidArgumentException();
				return new XVideoStream(src, codec, desc, num_opt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoStream> _create_vi_enc_stream_s(XStream * xstream, const string & media, const string & codec, const MediaVideoDesc & desc, uint num_opt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XVideoStream(stream, media, codec, desc, num_opt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XVideoFactory> _create_vi_factory(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XVideoFactory(); XE_TRY_OUTRO(0) }
			static SafePointer<XAudioTrackDesc> _create_au_desc(const string & codec, const MediaAudioDesc & desc, uint channel_layout, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				Audio::StreamDesc sd;
				ConvertToEngine(sd, desc);
				SafePointer<Media::AudioTrackFormatDesc> asd = new Media::AudioTrackFormatDesc(codec, sd, channel_layout);
				return new XAudioTrackDesc(asd);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XWave> _create_wave_buffer(const MediaAudioDesc & desc, uint64 length, ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XWave(desc, length); XE_TRY_OUTRO(0) }
			static SafePointer<XAudioEncodingSession> _create_au_dec_session(XAudioTrackDesc & desc, const MediaAudioDesc * desired, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XAudioEncodingSession(desc, desired);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioEncodingSession> _create_au_enc_session(const string & codec, const MediaAudioDesc & desc, uint numopt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XAudioEncodingSession(codec, desc, numopt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioStream> _create_au_dec_stream_t(XTrack * track, const MediaAudioDesc * desired, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = track->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XAudioStream(src, desired);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioStream> _create_au_dec_stream_c(XContainer * container, const MediaAudioDesc * desired, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XAudioStream(src, desired);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioStream> _create_au_dec_stream_s(XStream * xstream, const MediaAudioDesc * desired, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XAudioStream(stream, desired);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioStream> _create_au_enc_stream_c(XContainer * container, const string & codec, const MediaAudioDesc & desc, uint num_opt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSink();
				if (!src) throw InvalidArgumentException();
				return new XAudioStream(src, codec, desc, num_opt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioStream> _create_au_enc_stream_s(XStream * xstream, const string & media, const string & codec, const MediaAudioDesc & desc, uint num_opt, const uint * opt, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XAudioStream(stream, media, codec, desc, num_opt, opt);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XAudioFactory> _create_au_factory(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XAudioFactory(); XE_TRY_OUTRO(0) }
			static SafePointer<XSubtitlesTrackDesc> _create_ss_desc(const string & codec, const MediaSubtitlesDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Media::SubtitleTrackFormatDesc> sd = new Media::SubtitleTrackFormatDesc(codec, desc.scale, desc.flags);
				return new XSubtitlesTrackDesc(sd);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesEncodingSession> _create_ss_dec_session(XSubtitlesTrackDesc * desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XSubtitlesEncodingSession(*desc);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesEncodingSession> _create_ss_enc_session(const string & codec, const MediaSubtitlesDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new XSubtitlesEncodingSession(codec, desc);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesStream> _create_ss_dec_stream_t(XTrack * track, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = track->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XSubtitlesStream(src);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesStream> _create_ss_dec_stream_c(XContainer * container, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSource();
				if (!src) throw InvalidArgumentException();
				return new XSubtitlesStream(src);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesStream> _create_ss_dec_stream_s(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XSubtitlesStream(stream);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesStream> _create_ss_enc_stream_c(XContainer * container, const string & codec, const MediaSubtitlesDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto src = container->GetSink();
				if (!src) throw InvalidArgumentException();
				return new XSubtitlesStream(src, codec, desc);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XSubtitlesStream> _create_ss_enc_stream_s(XStream * xstream, const string & media, const string & codec, const MediaSubtitlesDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				return new XSubtitlesStream(stream, media, codec, desc);
				XE_TRY_OUTRO(0)
			}
		public:
			MultimediaExtension(void) {}
			virtual ~MultimediaExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"mm_ssdc") < 0) {
					if (string::Compare(routine_name, L"mm_aufd1") < 0) {
						if (string::Compare(routine_name, L"mm_audc") < 0) {
							if (string::Compare(routine_name, L"mm_aucf") < 0) {
								if (string::Compare(routine_name, L"mm_aucd") == 0) return reinterpret_cast<const void *>(&_create_au_enc_session);
							} else {
								if (string::Compare(routine_name, L"mm_aucfb") < 0) {
									if (string::Compare(routine_name, L"mm_aucf") == 0) return reinterpret_cast<const void *>(&_create_wave_buffer);
								} else {
									if (string::Compare(routine_name, L"mm_aucfb") == 0) return reinterpret_cast<const void *>(&_create_au_factory);
								}
							}
						} else {
							if (string::Compare(routine_name, L"mm_aufc1") < 0) {
								if (string::Compare(routine_name, L"mm_audp") < 0) {
									if (string::Compare(routine_name, L"mm_audc") == 0) return reinterpret_cast<const void *>(&_create_au_dec_session);
								} else {
									if (string::Compare(routine_name, L"mm_audp") == 0) return reinterpret_cast<const void *>(&_create_au_desc);
								}
							} else {
								if (string::Compare(routine_name, L"mm_aufc2") < 0) {
									if (string::Compare(routine_name, L"mm_aufc1") == 0) return reinterpret_cast<const void *>(&_create_au_enc_stream_c);
								} else {
									if (string::Compare(routine_name, L"mm_aufc2") == 0) return reinterpret_cast<const void *>(&_create_au_enc_stream_s);
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"mm_mdcr") < 0) {
							if (string::Compare(routine_name, L"mm_aufd3") < 0) {
								if (string::Compare(routine_name, L"mm_aufd2") < 0) {
									if (string::Compare(routine_name, L"mm_aufd1") == 0) return reinterpret_cast<const void *>(&_create_au_dec_stream_t);
								} else {
									if (string::Compare(routine_name, L"mm_aufd2") == 0) return reinterpret_cast<const void *>(&_create_au_dec_stream_c);
								}
							} else {
								if (string::Compare(routine_name, L"mm_aurud") < 0) {
									if (string::Compare(routine_name, L"mm_aufd3") == 0) return reinterpret_cast<const void *>(&_create_au_dec_stream_s);
								} else {
									if (string::Compare(routine_name, L"mm_aurud") == 0) return reinterpret_cast<const void *>(&Audio::Beep);
								}
							}
						} else {
							if (string::Compare(routine_name, L"mm_mmcr") < 0) {
								if (string::Compare(routine_name, L"mm_mmap") < 0) {
									if (string::Compare(routine_name, L"mm_mdcr") == 0) return reinterpret_cast<const void *>(&_create_metadata);
								} else {
									if (string::Compare(routine_name, L"mm_mmap") == 0) return reinterpret_cast<const void *>(&_open_container);
								}
							} else {
								if (string::Compare(routine_name, L"mm_sscd") < 0) {
									if (string::Compare(routine_name, L"mm_mmcr") == 0) return reinterpret_cast<const void *>(&_create_container);
								} else {
									if (string::Compare(routine_name, L"mm_sscd") == 0) return reinterpret_cast<const void *>(&_create_ss_enc_session);
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"mm_vicfb") < 0) {
						if (string::Compare(routine_name, L"mm_ssfd1") < 0) {
							if (string::Compare(routine_name, L"mm_ssfc1") < 0) {
								if (string::Compare(routine_name, L"mm_ssdp") < 0) {
									if (string::Compare(routine_name, L"mm_ssdc") == 0) return reinterpret_cast<const void *>(&_create_ss_dec_session);
								} else {
									if (string::Compare(routine_name, L"mm_ssdp") == 0) return reinterpret_cast<const void *>(&_create_ss_desc);
								}
							} else {
								if (string::Compare(routine_name, L"mm_ssfc2") < 0) {
									if (string::Compare(routine_name, L"mm_ssfc1") == 0) return reinterpret_cast<const void *>(&_create_ss_enc_stream_c);
								} else {
									if (string::Compare(routine_name, L"mm_ssfc2") == 0) return reinterpret_cast<const void *>(&_create_ss_enc_stream_s);
								}
							}
						} else {
							if (string::Compare(routine_name, L"mm_ssfd3") < 0) {
								if (string::Compare(routine_name, L"mm_ssfd2") < 0) {
									if (string::Compare(routine_name, L"mm_ssfd1") == 0) return reinterpret_cast<const void *>(&_create_ss_dec_stream_t);
								} else {
									if (string::Compare(routine_name, L"mm_ssfd2") == 0) return reinterpret_cast<const void *>(&_create_ss_dec_stream_c);
								}
							} else {
								if (string::Compare(routine_name, L"mm_vicd") < 0) {
									if (string::Compare(routine_name, L"mm_ssfd3") == 0) return reinterpret_cast<const void *>(&_create_ss_dec_stream_s);
								} else {
									if (string::Compare(routine_name, L"mm_vicd") == 0) return reinterpret_cast<const void *>(&_create_vi_enc_session);
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"mm_vifc2") < 0) {
							if (string::Compare(routine_name, L"mm_vidp") < 0) {
								if (string::Compare(routine_name, L"mm_vidc") < 0) {
									if (string::Compare(routine_name, L"mm_vicfb") == 0) return reinterpret_cast<const void *>(&_create_vi_factory);
								} else {
									if (string::Compare(routine_name, L"mm_vidc") == 0) return reinterpret_cast<const void *>(&_create_vi_dec_session);
								}
							} else {
								if (string::Compare(routine_name, L"mm_vifc1") < 0) {
									if (string::Compare(routine_name, L"mm_vidp") == 0) return reinterpret_cast<const void *>(&_create_vi_desc);
								} else {
									if (string::Compare(routine_name, L"mm_vifc1") == 0) return reinterpret_cast<const void *>(&_create_vi_enc_stream_c);
								}
							}
						} else {
							if (string::Compare(routine_name, L"mm_vifd2") < 0) {
								if (string::Compare(routine_name, L"mm_vifd1") < 0) {
									if (string::Compare(routine_name, L"mm_vifc2") == 0) return reinterpret_cast<const void *>(&_create_vi_enc_stream_s);
								} else {
									if (string::Compare(routine_name, L"mm_vifd1") == 0) return reinterpret_cast<const void *>(&_create_vi_dec_stream_t);
								}
							} else {
								if (string::Compare(routine_name, L"mm_vifd3") < 0) {
									if (string::Compare(routine_name, L"mm_vifd2") == 0) return reinterpret_cast<const void *>(&_create_vi_dec_stream_c);
								} else {
									if (string::Compare(routine_name, L"mm_vifd3") == 0) return reinterpret_cast<const void *>(&_create_vi_dec_stream_s);
								}
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		void ActivateMultimedia(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new MultimediaExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}

		SafePointer<Object> XVideoStream::GetTrack(ErrorContext & ectx) noexcept
		{
			XE_TRY_INTRO
			if (_dec) return new XTrack(_dec->GetSourceTrack());
			else return new XTrack(_enc->GetDestinationSink());
			XE_TRY_OUTRO(0)
		}
		SafePointer<Object> XAudioStream::GetTrack(ErrorContext & ectx) noexcept
		{
			XE_TRY_INTRO
			if (_dec) return new XTrack(_dec->GetSourceTrack());
			else return new XTrack(_enc->GetDestinationSink());
			XE_TRY_OUTRO(0)
		}
		SafePointer<Object> XSubtitlesStream::GetTrack(ErrorContext & ectx) noexcept
		{
			XE_TRY_INTRO
			if (_dec) return new XTrack(_dec->GetSourceTrack());
			else return new XTrack(_enc->GetDestinationSink());
			XE_TRY_OUTRO(0)
		}
	}
}
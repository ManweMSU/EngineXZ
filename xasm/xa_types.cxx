#include "xa_types.h"

namespace Engine
{
	namespace XA
	{
		namespace Encoder
		{
			void EncodeALInt(Streaming::Stream * stream, uint64 value)
			{
				bool inverted = false;
				uint64 venc = ~value;
				if (venc < value) inverted = true; else venc = value;
				uint8 write = venc & 0x3F;
				if (inverted) write |= 0x40;
				if (venc & ~0x3FULL) write |= 0x80;
				stream->Write(&write, 1);
				venc >>= 6;
				while (venc) {
					write = venc & 0x7F;
					if (venc & ~0x7FULL) write |= 0x80;
					stream->Write(&write, 1);
					venc >>= 7;
				}
			}
			void EncodeALInt(Streaming::Stream * stream, int64 value) { EncodeALInt(stream, uint64(value)); }
			void EncodeALInt(Streaming::Stream * stream, int32 value) { EncodeALInt(stream, int64(value)); }
			void EncodeALInt(Streaming::Stream * stream, uint32 value) { EncodeALInt(stream, int32(value)); }
			uint64 DecodeALInt(Streaming::Stream * stream)
			{
				bool invert = false;
				uint64 result;
				uint64 offset;
				uint8 read;
				stream->Read(&read, 1);
				if (read & 0x40) invert = true;
				result = read & 0x3F;
				offset = 6;
				while (read & 0x80) {
					if (offset >= 64) throw InvalidFormatException();
					stream->Read(&read, 1);
					result |= uint64(read & 0x7F) << offset;
					offset += 7;
				}
				return invert ? ~result : result;
			}
			void EncodeSize(Streaming::Stream * stream, const ObjectSize & size)
			{
				EncodeALInt(stream, size.num_bytes);
				EncodeALInt(stream, size.num_words);
			}
			ObjectSize DecodeSize(Streaming::Stream * stream)
			{
				ObjectSize result;
				result.num_bytes = DecodeALInt(stream);
				result.num_words = DecodeALInt(stream);
				return result;
			}
			void EncodeSpec(Streaming::Stream * stream, const ArgumentSpecification & spec)
			{
				EncodeSize(stream, spec.size);
				EncodeALInt(stream, uint(spec.semantics));
			}
			ArgumentSpecification DecodeSpec(Streaming::Stream * stream)
			{
				ArgumentSpecification result;
				result.size = DecodeSize(stream);
				result.semantics = static_cast<ArgumentSemantics>(DecodeALInt(stream));
				return result;
			}
			void EncodeString(Streaming::Stream * stream, const string & str)
			{
				int length = str.GetEncodedLength(Encoding::UTF8);
				SafePointer<DataBlock> data = str.EncodeSequence(Encoding::UTF8, false);
				EncodeALInt(stream, length);
				stream->WriteArray(data);
			}
			string DecodeString(Streaming::Stream * stream)
			{
				int length;
				length = DecodeALInt(stream);
				SafePointer<DataBlock> data = stream->ReadBlock(length);
				return string(data->GetBuffer(), length, Encoding::UTF8);
			}
			template<class V, class F> void EncodeVolume(Streaming::Stream * stream, const V & volume, F f)
			{
				int length = 0;
				for (auto & e : volume) length++;
				EncodeALInt(stream, length);
				for (auto & e : volume) f(e);
			}
			template<class F> void DecodeVolume(Streaming::Stream * stream, F f)
			{
				int length = DecodeALInt(stream);
				for (int i = 0; i < length; i++) f();
			}
			void EncodeData(Streaming::Stream * stream, const DataBlock & data)
			{
				int length = data.Length();
				stream->Write(&length, 4);
				stream->Write(data.GetBuffer(), data.Length());
			}
			void DecodeData(Streaming::Stream * stream, DataBlock & data)
			{
				int length;
				stream->Read(&length, 4);
				data.SetLength(length);
				stream->Read(data.GetBuffer(), length);
			}
			void EncodeLongData(Streaming::Stream * stream, const Array<uint32> & data)
			{
				int length = data.Length();
				stream->Write(&length, 4);
				stream->Write(data.GetBuffer(), data.Length() * 4);
			}
			void DecodeLongData(Streaming::Stream * stream, Array<uint32> & data)
			{
				int length;
				stream->Read(&length, 4);
				if (length > 0x1FFFFFFF) throw InvalidFormatException();
				data.SetLength(length);
				stream->Read(data.GetBuffer(), length * 4);
			}
			void EncodeFinalizer(Streaming::Stream * stream, const FinalizerReference & final)
			{
				EncodeALInt(stream, final.final.qword);
				EncodeVolume(stream, final.final_args, [stream](const ObjectReference & ref) {
					EncodeALInt(stream, ref.qword);
				});
			}
			void DecodeFinalizer(Streaming::Stream * stream, FinalizerReference & final)
			{
				final.final.qword = DecodeALInt(stream);
				DecodeVolume(stream, [stream, f = &final]() {
					ObjectReference ref;
					ref.qword = DecodeALInt(stream);
					f->final_args << ref;
				});
			}
			void EncodeTree(Streaming::Stream * stream, const ExpressionTree & tree)
			{
				EncodeALInt(stream, tree.self.qword);
				if (tree.self.ref_flags & ReferenceFlagInvoke) {
					EncodeVolume(stream, tree.inputs, [stream](const ExpressionTree & st) {
						EncodeTree(stream, st);
					});
					EncodeVolume(stream, tree.input_specs, [stream](const ArgumentSpecification & spec) {
						EncodeSpec(stream, spec);
					});
					EncodeSpec(stream, tree.retval_spec);
					EncodeFinalizer(stream, tree.retval_final);
				}
			}
			void DecodeTree(Streaming::Stream * stream, ExpressionTree & tree)
			{
				tree.self.qword = DecodeALInt(stream);
				if (tree.self.ref_flags & ReferenceFlagInvoke) {
					DecodeVolume(stream, [stream, t = &tree]() {
						ExpressionTree st;
						DecodeTree(stream, st);
						t->inputs << st;
					});
					DecodeVolume(stream, [stream, t = &tree]() {
						t->input_specs << DecodeSpec(stream);
					});
					tree.retval_spec = DecodeSpec(stream);
					DecodeFinalizer(stream, tree.retval_final);
				}
			}
		}
		FinalizerReference::FinalizerReference(void) : final_args(1) {}
		ExpressionTree::ExpressionTree(void) : inputs(1), input_specs(1) {}
		Function::Function(void) : extrefs(0x20), data(0x100), inputs(0x10), instset(0x100) {}
		void Function::Clear(void)
		{
			extrefs.Clear(); data.Clear(); retval.semantics = ArgumentSemantics::Unknown;
			retval.size.num_bytes = retval.size.num_words = 0; inputs.Clear(); instset.Clear();
		}
		void Function::Save(Streaming::Stream * dest)
		{
			Encoder::EncodeVolume(dest, extrefs, [dest](const string & e) {
				Encoder::EncodeString(dest, e);
			});
			Encoder::EncodeData(dest, data);
			Encoder::EncodeSpec(dest, retval);
			Encoder::EncodeVolume(dest, inputs, [dest](const ArgumentSpecification & spec) {
				Encoder::EncodeSpec(dest, spec);
			});
			Encoder::EncodeVolume(dest, instset, [dest](const Statement & s) {
				dest->Write(&s.opcode, 4);
				Encoder::EncodeSize(dest, s.attachment);
				Encoder::EncodeFinalizer(dest, s.attachment_final);
				Encoder::EncodeTree(dest, s.tree);
			});
		}
		void Function::Load(Streaming::Stream * src)
		{
			Clear();
			Encoder::DecodeVolume(src, [src, this]() {
				extrefs << Encoder::DecodeString(src);
			});
			Encoder::DecodeData(src, data);
			retval = Encoder::DecodeSpec(src);
			Encoder::DecodeVolume(src, [src, this]() {
				inputs << Encoder::DecodeSpec(src);
			});
			Encoder::DecodeVolume(src, [src, this]() {
				Statement stat;
				src->Read(&stat.opcode, 4);
				stat.attachment = Encoder::DecodeSize(src);
				Encoder::DecodeFinalizer(src, stat.attachment_final);
				Encoder::DecodeTree(src, stat.tree);
				instset << stat;
			});
		}
		TranslatedFunction::TranslatedFunction(void) : data(0x1000), code(0x1000), code_reloc(0x100), data_reloc(0x100) {}
		void TranslatedFunction::Clear(void) { data.Clear(); code.Clear(); extrefs.Clear(); data_reloc.Clear(); code_reloc.Clear(); }
		void TranslatedFunction::Save(Streaming::Stream * dest)
		{
			Encoder::EncodeData(dest, data);
			Encoder::EncodeData(dest, code);
			Encoder::EncodeVolume(dest, extrefs, [dest](const Volumes::KeyValuePair< string, Array<uint32> > & e) {
				Encoder::EncodeString(dest, e.key);
				Encoder::EncodeLongData(dest, e.value);
			});
			Encoder::EncodeLongData(dest, data_reloc);
			Encoder::EncodeLongData(dest, code_reloc);
		}
		void TranslatedFunction::Load(Streaming::Stream * src)
		{
			Clear();
			Encoder::DecodeData(src, data);
			Encoder::DecodeData(src, code);
			Encoder::DecodeVolume(src, [src, this]() {
				Array<uint32> reloc(1);
				auto key = Encoder::DecodeString(src);
				Encoder::DecodeLongData(src, reloc);
				extrefs.Append(key, reloc);
			});
			Encoder::DecodeLongData(src, data_reloc);
			Encoder::DecodeLongData(src, code_reloc);
		}
	}
}
/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ScriptJson.cpp : converts script exports between .scb and JSON; an unedited JSON converts back byte for byte

#include "StdAfx.h"
#include "ScriptJson.h"

#include <windows.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <new>
#include <vector>

namespace
{

const char *const PARAMETER_TYPES[] =
{
	"INT", "REAL", "SCRIPT", "TEAM", "COUNTER", "FLAG", "COMPARISON", "WAYPOINT", "BOOLEAN",
	"TRIGGER_AREA", "TEXT_STRING", "SIDE", "SOUND", "SCRIPT_SUBROUTINE", "UNIT", "OBJECT_TYPE",
	"COORD3D", "ANGLE", "TEAM_STATE", "RELATION", "AI_MOOD", "DIALOG", "MUSIC", "MOVIE",
	"WAYPOINT_PATH", "LOCALIZED_TEXT", "BRIDGE", "KIND_OF_PARAM", "ATTACK_PRIORITY_SET",
	"RADAR_EVENT_TYPE", "SPECIAL_POWER", "SCIENCE", "UPGRADE", "COMMANDBUTTON_ABILITY", "BOUNDARY",
	"BUILDABLE", "SURFACES_ALLOWED", "SHAKE_INTENSITY", "COMMAND_BUTTON", "FONT_NAME",
	"OBJECT_STATUS", "COMMANDBUTTON_ALL_ABILITIES", "SKIRMISH_WAYPOINT_PATH", "COLOR", "EMOTICON",
	"OBJECT_PANEL_FLAG", "FACTION_NAME", "OBJECT_TYPE_LIST", "REVEALNAME", "SCIENCE_AVAILABILITY",
	"LEFT_OR_RIGHT", "PERCENT", "KD_RATIO", "SELECTION_MODE",
};
const int NUM_PARAMETER_TYPES = sizeof(PARAMETER_TYPES) / sizeof(PARAMETER_TYPES[0]);
const int PARAM_COMPARISON = 6;
const int PARAM_COORD3D = 16;

const char *const COMPARISONS[] = { "LESS_THAN", "LESS_EQUAL", "EQUAL", "GREATER_EQUAL", "GREATER", "NOT_EQUAL" };
const int NUM_COMPARISONS = 6;

enum { DICT_BOOL, DICT_INT, DICT_REAL, DICT_ASCIISTRING, DICT_UNICODESTRING };

// Versions the engine writes today, used for items the JSON leaves them out of
const int CONDITION_VERSION = 4;
const int ACTION_VERSION = 2;
const int SCRIPT_VERSION = 2;
const int GROUP_VERSION = 2;
const int OR_VERSION = 1;
const int LIST_VERSION = 1;
const int OBJECT_VERSION = 3;
const int CONDITION_KEY_VERSION = 4;
const int ACTION_KEY_VERSION = 2;

struct ConvertError
{
	std::string message;
	explicit ConvertError(const std::string &m) : message(m) {}
};

std::string format(const char *fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	_vsnprintf(buf, sizeof(buf) - 1, fmt, args);
	va_end(args);
	buf[sizeof(buf) - 1] = 0;
	return buf;
}

// ------------------------------------------------------------------------------------------------
// Text encodings

void appendUtf8(std::string &out, unsigned int cp)
{
	if (cp < 0x80)
	{
		out += (char)cp;
	}
	else if (cp < 0x800)
	{
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000)
	{
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else
	{
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
}

std::vector<unsigned int> decodeUtf8(const std::string &s)
{
	std::vector<unsigned int> cps;
	size_t i = 0;
	while (i < s.size())
	{
		unsigned char c = (unsigned char)s[i];
		unsigned int cp;
		int extra;
		if (c < 0x80)
		{
			cp = c;
			extra = 0;
		}
		else if ((c & 0xE0) == 0xC0)
		{
			cp = c & 0x1F;
			extra = 1;
		}
		else if ((c & 0xF0) == 0xE0)
		{
			cp = c & 0x0F;
			extra = 2;
		}
		else
		{
			cp = c & 0x07;
			extra = 3;
		}
		++i;
		for (int k = 0; k < extra && i < s.size(); ++k, ++i)
		{
			cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
		}
		cps.push_back(cp);
	}
	return cps;
}

std::string latin1ToUtf8(const char *p, size_t n)
{
	std::string out;
	out.reserve(n);
	for (size_t i = 0; i < n; ++i)
	{
		appendUtf8(out, (unsigned char)p[i]);
	}
	return out;
}

std::string utf8ToLatin1(const std::string &s)
{
	std::vector<unsigned int> cps = decodeUtf8(s);
	std::string out;
	out.reserve(cps.size());
	for (size_t i = 0; i < cps.size(); ++i)
	{
		if (cps[i] > 0xFF)
		{
			throw ConvertError(format("\"%.60s\" has a character outside Latin-1", s.c_str()));
		}
		out += (char)cps[i];
	}
	return out;
}

// ------------------------------------------------------------------------------------------------
// Numbers, formatted as Python's repr() so the JSON text matches Python's json module

std::string pythonRepr(double d)
{
	if (d != d)
	{
		return "NaN";
	}
	if (d == HUGE_VAL)
	{
		return "Infinity";
	}
	if (d == -HUGE_VAL)
	{
		return "-Infinity";
	}
	if (d == 0.0)
	{
		return _copysign(1.0, d) < 0 ? "-0.0" : "0.0";
	}

	char buf[64];
	for (int p = 1; p <= 17; ++p)
	{
		_snprintf(buf, sizeof(buf), "%.*e", p - 1, d);
		if (strtod(buf, nullptr) == d)
		{
			break;
		}
	}

	std::string s = buf;
	std::string sign;
	if (s[0] == '-')
	{
		sign = "-";
		s.erase(0, 1);
	}
	size_t e = s.find('e');
	int exp10 = atoi(s.c_str() + e + 1);
	std::string digits;
	for (size_t i = 0; i < e; ++i)
	{
		if (s[i] != '.')
		{
			digits += s[i];
		}
	}
	while (digits.size() > 1 && digits[digits.size() - 1] == '0')
	{
		digits.erase(digits.size() - 1);
	}

	int decpt = exp10 + 1;
	if (decpt > -4 && decpt <= 16)
	{
		if (decpt <= 0)
		{
			return sign + "0." + std::string(-decpt, '0') + digits;
		}
		if (decpt >= (int)digits.size())
		{
			return sign + digits + std::string(decpt - digits.size(), '0') + ".0";
		}
		return sign + digits.substr(0, decpt) + "." + digits.substr(decpt);
	}

	std::string out = sign + digits.substr(0, 1);
	if (digits.size() > 1)
	{
		out += "." + digits.substr(1);
	}
	_snprintf(buf, sizeof(buf), "e%c%02d", exp10 < 0 ? '-' : '+', exp10 < 0 ? -exp10 : exp10);
	return out + buf;
}

// Shortest decimal that packs back to the same float32
std::string realRepr(float f)
{
	for (int digits = 6; digits <= 9; ++digits)
	{
		char buf[64];
		_snprintf(buf, sizeof(buf), "%.*g", digits, (double)f);
		double d = strtod(buf, nullptr);
		float back = (float)d;
		if (memcmp(&back, &f, sizeof(float)) == 0)
		{
			return pythonRepr(d);
		}
	}
	return pythonRepr((double)f);
}

// ------------------------------------------------------------------------------------------------
// Base64

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const unsigned char *p, size_t n)
{
	std::string out;
	out.reserve((n + 2) / 3 * 4);
	for (size_t i = 0; i < n; i += 3)
	{
		unsigned int v = p[i] << 16;
		if (i + 1 < n)
		{
			v |= p[i + 1] << 8;
		}
		if (i + 2 < n)
		{
			v |= p[i + 2];
		}
		out += B64[(v >> 18) & 63];
		out += B64[(v >> 12) & 63];
		out += i + 1 < n ? B64[(v >> 6) & 63] : '=';
		out += i + 2 < n ? B64[v & 63] : '=';
	}
	return out;
}

std::string base64Decode(const std::string &s)
{
	std::string out;
	unsigned int v = 0;
	int bits = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const char *pos = strchr(B64, s[i]);
		if (s[i] == '=' || pos == nullptr || s[i] == 0)
		{
			continue;
		}
		v = (v << 6) | (unsigned int)(pos - B64);
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			out += (char)((v >> bits) & 0xFF);
		}
	}
	return out;
}

// ------------------------------------------------------------------------------------------------
// JSON writer, laid out like Python's json.dumps(indent=1, ensure_ascii=False)

class JsonOut
{
public:
	std::string text;

	void open(char c)
	{
		text += c;
		m_counts.push_back(0);
	}

	void close(char c)
	{
		int n = m_counts.back();
		m_counts.pop_back();
		if (n)
		{
			newline();
		}
		text += c;
	}

	void item()
	{
		if (m_counts.back()++)
		{
			text += ',';
		}
		newline();
	}

	void key(const char *k)
	{
		item();
		string(k);
		text += ": ";
	}

	void string(const std::string &utf8)
	{
		text += '"';
		for (size_t i = 0; i < utf8.size(); ++i)
		{
			unsigned char c = (unsigned char)utf8[i];
			switch (c)
			{
				case '"': text += "\\\""; break;
				case '\\': text += "\\\\"; break;
				case '\n': text += "\\n"; break;
				case '\r': text += "\\r"; break;
				case '\t': text += "\\t"; break;
				case '\b': text += "\\b"; break;
				case '\f': text += "\\f"; break;
				default:
					if (c < 0x20)
					{
						text += format("\\u%04x", c);
					}
					else
					{
						text += (char)c;
					}
			}
		}
		text += '"';
	}

	void integer(long long v)
	{
		text += format("%lld", v);
	}

	void boolean(bool v)
	{
		text += v ? "true" : "false";
	}

	void real(float v)
	{
		text += realRepr(v);
	}

	// Lets a chunk that fails to decode fall back to raw
	size_t mark() const
	{
		return text.size();
	}

	size_t depth() const
	{
		return m_counts.size();
	}

	void rewind(size_t pos, size_t depth, int count)
	{
		text.resize(pos);
		m_counts.resize(depth);
		m_counts.back() = count;
	}

	int count() const
	{
		return m_counts.back();
	}

private:
	void newline()
	{
		text += '\n';
		text.append(m_counts.size(), ' ');
	}

	std::vector<int> m_counts;
};

// ------------------------------------------------------------------------------------------------
// .scb reader

typedef std::map<unsigned int, std::string> NameMap;

class Reader
{
public:
	Reader(const std::string &data, const NameMap &names, size_t pos, size_t end)
		: m_d(data), m_names(names), p(pos), end(end)
	{
	}

	size_t left() const
	{
		return end - p;
	}

	void need(size_t n)
	{
		if (p + n > end)
		{
			throw ConvertError(format("read past end of chunk at offset %u", (unsigned int)p));
		}
	}

	int i8()
	{
		need(1);
		return (signed char)m_d[p++];
	}

	unsigned int u16()
	{
		need(2);
		unsigned int v = (unsigned char)m_d[p] | ((unsigned char)m_d[p + 1] << 8);
		p += 2;
		return v;
	}

	int i32()
	{
		need(4);
		int v;
		memcpy(&v, m_d.data() + p, 4);
		p += 4;
		return v;
	}

	float f32()
	{
		need(4);
		float v;
		memcpy(&v, m_d.data() + p, 4);
		p += 4;
		return v;
	}

	std::string ascii()
	{
		unsigned int n = u16();
		need(n);
		std::string s = latin1ToUtf8(m_d.data() + p, n);
		p += n;
		return s;
	}

	std::string unicode()
	{
		unsigned int n = u16();
		need(2 * n);
		std::string out;
		for (unsigned int i = 0; i < n; ++i)
		{
			unsigned int c = (unsigned char)m_d[p + 2 * i] | ((unsigned char)m_d[p + 2 * i + 1] << 8);
			if (c >= 0xD800 && c < 0xDC00 && i + 1 < n)
			{
				unsigned int lo = (unsigned char)m_d[p + 2 * i + 2] | ((unsigned char)m_d[p + 2 * i + 3] << 8);
				if (lo >= 0xDC00 && lo < 0xE000)
				{
					c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
					++i;
				}
			}
			appendUtf8(out, c);
		}
		p += 2 * n;
		return out;
	}

	std::string name(unsigned int id) const
	{
		NameMap::const_iterator it = m_names.find(id);
		return it == m_names.end() ? std::string("?") : it->second;
	}

	// Steps through the chunks inside this one; returns false when none is left
	bool nextChunk(std::string &label, int &ver, Reader *&body)
	{
		if (left() < 10)
		{
			return false;
		}
		unsigned int id = (unsigned int)i32();
		ver = (int)u16();
		int size = i32();
		if (size < 0 || p + size > end)
		{
			throw ConvertError(format("chunk at offset %u runs past its parent", (unsigned int)p));
		}
		label = name(id);
		delete body;
		body = new Reader(m_d, m_names, p, p + size);
		p += size;
		return true;
	}

	const std::string &m_d;
	const NameMap &m_names;
	size_t p;
	size_t end;
};

// Owns the body Reader that nextChunk hands out
struct ChunkIter
{
	Reader &parent;
	Reader *body;
	std::string label;
	int ver;

	explicit ChunkIter(Reader &r) : parent(r), body(nullptr), ver(0) {}
	~ChunkIter() { delete body; }
	bool next() { return parent.nextChunk(label, ver, body); }
};

void emitRaw(JsonOut &o, const std::string &label, int ver, const Reader &r)
{
	o.open('{');
	o.key("chunk");
	o.string(label);
	o.key("version");
	o.integer(ver);
	o.key("raw");
	o.string(base64Encode((const unsigned char *)r.m_d.data() + r.p, r.end - r.p));
	o.close('}');
}

void emitDict(JsonOut &o, Reader &r)
{
	o.open('{');
	unsigned int n = r.u16();
	for (unsigned int i = 0; i < n; ++i)
	{
		int keyAndType = r.i32();
		std::string key = r.name((unsigned int)keyAndType >> 8);
		int t = keyAndType & 0xFF;
		o.key(key.c_str());
		switch (t)
		{
			case DICT_BOOL: o.boolean(r.i8() != 0); break;
			case DICT_INT: o.integer(r.i32()); break;
			case DICT_REAL: o.real(r.f32()); break;
			case DICT_ASCIISTRING: o.string(r.ascii()); break;
			case DICT_UNICODESTRING:
				o.open('{');
				o.key("unicode");
				o.string(r.unicode());
				o.close('}');
				break;
			default:
				throw ConvertError(format("unknown dict type %d for %s", t, key.c_str()));
		}
	}
	o.close('}');
}

void emitParameter(JsonOut &o, Reader &r)
{
	int t = r.i32();
	o.open('{');
	o.key("type");
	if (t >= 0 && t < NUM_PARAMETER_TYPES)
	{
		o.string(PARAMETER_TYPES[t]);
	}
	else
	{
		o.integer(t);
	}
	if (t == PARAM_COORD3D)
	{
		o.key("coord");
		o.open('[');
		for (int k = 0; k < 3; ++k)
		{
			o.item();
			o.real(r.f32());
		}
		o.close(']');
	}
	else
	{
		int iv = r.i32();
		o.key("int");
		o.integer(iv);
		o.key("real");
		o.real(r.f32());
		o.key("string");
		o.string(r.ascii());
		if (t == PARAM_COMPARISON && iv >= 0 && iv < NUM_COMPARISONS)
		{
			o.key("comparison");
			o.string(COMPARISONS[iv]);
		}
	}
	o.close('}');
}

void emitConditionOrAction(JsonOut &o, int ver, Reader &r, int keyVersion)
{
	o.open('{');
	o.key("type");
	o.integer(r.i32());
	o.key("version");
	o.integer(ver);
	if (ver >= keyVersion)
	{
		o.key("name");
		o.string(r.name((unsigned int)r.i32() >> 8));
	}
	o.key("parameters");
	o.open('[');
	int n = r.i32();
	for (int i = 0; i < n; ++i)
	{
		o.item();
		emitParameter(o, r);
	}
	o.close(']');
	o.close('}');
}

void emitBoolField(JsonOut &o, const char *key, Reader &r)
{
	o.key(key);
	o.boolean(r.i8() != 0);
}

void emitScript(JsonOut &o, int ver, Reader &r)
{
	o.open('{');
	o.key("name");
	o.string(r.ascii());
	o.key("comment");
	o.string(r.ascii());
	o.key("conditionComment");
	o.string(r.ascii());
	o.key("actionComment");
	o.string(r.ascii());
	emitBoolField(o, "active", r);
	emitBoolField(o, "oneShot", r);
	emitBoolField(o, "easy", r);
	emitBoolField(o, "normal", r);
	emitBoolField(o, "hard", r);
	emitBoolField(o, "subroutine", r);
	o.key("version");
	o.integer(ver);
	if (ver >= 2)
	{
		o.key("delayEvaluationSeconds");
		o.integer(r.i32());
	}

	// The JSON groups the sub-chunks by kind, so each kind is a separate pass
	size_t start = r.p;
	const char *const kinds[] = { "or", "actions", "falseActions" };
	for (int pass = 0; pass < 3; ++pass)
	{
		r.p = start;
		o.key(kinds[pass]);
		o.open('[');
		ChunkIter it(r);
		while (it.next())
		{
			if (pass == 0 && it.label == "OrCondition" && it.ver == OR_VERSION)
			{
				o.item();
				o.open('[');
				ChunkIter cond(*it.body);
				while (cond.next())
				{
					if (cond.label == "Condition")
					{
						o.item();
						emitConditionOrAction(o, cond.ver, *cond.body, CONDITION_KEY_VERSION);
					}
				}
				o.close(']');
			}
			else if (pass == 1 && it.label == "ScriptAction")
			{
				o.item();
				emitConditionOrAction(o, it.ver, *it.body, ACTION_KEY_VERSION);
			}
			else if (pass == 2 && it.label == "ScriptActionFalse")
			{
				o.item();
				emitConditionOrAction(o, it.ver, *it.body, ACTION_KEY_VERSION);
			}
		}
		o.close(']');
	}

	r.p = start;
	bool any = false;
	ChunkIter it(r);
	while (it.next())
	{
		bool known = (it.label == "OrCondition" && it.ver == OR_VERSION) || it.label == "ScriptAction" || it.label == "ScriptActionFalse";
		if (!known)
		{
			if (!any)
			{
				o.key("unknown");
				o.open('[');
				any = true;
			}
			o.item();
			emitRaw(o, it.label, it.ver, *it.body);
		}
	}
	if (any)
	{
		o.close(']');
	}
	o.close('}');
}

void emitScriptList(JsonOut &o, int ver, Reader &r)
{
	o.open('{');
	o.key("version");
	o.integer(ver);
	size_t start = r.p;

	o.key("scripts");
	o.open('[');
	{
		ChunkIter it(r);
		while (it.next())
		{
			if (it.label == "Script")
			{
				o.item();
				emitScript(o, it.ver, *it.body);
			}
		}
	}
	o.close(']');

	r.p = start;
	o.key("groups");
	o.open('[');
	{
		ChunkIter it(r);
		while (it.next())
		{
			if (it.label != "ScriptGroup")
			{
				continue;
			}
			Reader &g = *it.body;
			o.item();
			o.open('{');
			o.key("name");
			o.string(g.ascii());
			emitBoolField(o, "active", g);
			o.key("version");
			o.integer(it.ver);
			if (it.ver >= 2)
			{
				emitBoolField(o, "subroutine", g);
			}
			o.key("scripts");
			o.open('[');
			ChunkIter sc(g);
			while (sc.next())
			{
				if (sc.label == "Script")
				{
					o.item();
					emitScript(o, sc.ver, *sc.body);
				}
			}
			o.close(']');
			o.close('}');
		}
	}
	o.close(']');

	r.p = start;
	bool any = false;
	ChunkIter it(r);
	while (it.next())
	{
		if (it.label != "Script" && it.label != "ScriptGroup")
		{
			if (!any)
			{
				o.key("unknown");
				o.open('[');
				any = true;
			}
			o.item();
			emitRaw(o, it.label, it.ver, *it.body);
		}
	}
	if (any)
	{
		o.close(']');
	}
	o.close('}');
}

// Returns false when the chunk has no decoder, so the caller writes it raw
bool emitTopChunk(JsonOut &o, const std::string &label, int ver, Reader &r)
{
	if (label == "PlayerScriptsList")
	{
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("scriptLists");
		o.open('[');
		ChunkIter it(r);
		while (it.next())
		{
			if (it.label != "ScriptList")
			{
				throw ConvertError("unexpected " + it.label + " chunk");
			}
			o.item();
			emitScriptList(o, it.ver, *it.body);
		}
		o.close(']');
		o.close('}');
		return true;
	}
	if (label == "ScriptsPlayers" && ver >= 2)
	{
		int doSides = r.i32();
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("exportSides");
		o.boolean(doSides != 0);
		o.key("players");
		o.open('[');
		int n = r.i32();
		for (int i = 0; i < n; ++i)
		{
			o.item();
			o.open('{');
			o.key("name");
			o.string(r.ascii());
			if (doSides)
			{
				o.key("dict");
				emitDict(o, r);
			}
			o.close('}');
		}
		o.close(']');
		o.close('}');
		return true;
	}
	if (label == "ObjectsList" && ver >= 3)
	{
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("objects");
		o.open('[');
		ChunkIter it(r);
		while (it.next())
		{
			o.item();
			if (it.label != "Object")
			{
				emitRaw(o, it.label, it.ver, *it.body);
				continue;
			}
			Reader &c = *it.body;
			o.open('{');
			o.key("version");
			o.integer(it.ver);
			o.key("location");
			o.open('[');
			for (int k = 0; k < 3; ++k)
			{
				o.item();
				o.real(c.f32());
			}
			o.close(']');
			o.key("angle");
			o.real(c.f32());
			o.key("flags");
			o.integer(c.i32());
			o.key("name");
			o.string(c.ascii());
			o.key("properties");
			emitDict(o, c);
			o.close('}');
		}
		o.close(']');
		o.close('}');
		return true;
	}
	if (label == "PolygonTriggers" && ver >= 3)
	{
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("triggers");
		o.open('[');
		int n = r.i32();
		for (int i = 0; i < n; ++i)
		{
			o.item();
			o.open('{');
			o.key("name");
			o.string(r.ascii());
			o.key("id");
			o.integer(r.i32());
			emitBoolField(o, "water", r);
			emitBoolField(o, "river", r);
			o.key("riverStart");
			o.integer(r.i32());
			o.key("points");
			o.open('[');
			int points = r.i32();
			for (int k = 0; k < points; ++k)
			{
				o.item();
				o.open('[');
				for (int c = 0; c < 3; ++c)
				{
					o.item();
					o.integer(r.i32());
				}
				o.close(']');
			}
			o.close(']');
			o.close('}');
		}
		o.close(']');
		o.close('}');
		return true;
	}
	if (label == "ScriptTeams")
	{
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("teams");
		o.open('[');
		while (r.left() > 0)
		{
			o.item();
			emitDict(o, r);
		}
		o.close(']');
		o.close('}');
		return true;
	}
	if (label == "WaypointsList")
	{
		o.open('{');
		o.key("chunk");
		o.string(label);
		o.key("version");
		o.integer(ver);
		o.key("links");
		o.open('[');
		int n = r.i32();
		for (int i = 0; i < n; ++i)
		{
			o.item();
			o.open('[');
			o.item();
			o.integer(r.i32());
			o.item();
			o.integer(r.i32());
			o.close(']');
		}
		o.close(']');
		o.close('}');
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// JSON reader

struct JValue
{
	enum Kind { JNULL, JBOOL, JNUMBER, JSTRING, JARRAY, JOBJECT };

	Kind kind;
	bool isInt;
	bool b;
	double num;
	long long inum;
	std::string str;
	std::vector<JValue> items;
	std::vector<std::string> keys;

	JValue() : kind(JNULL), isInt(false), b(false), num(0.0), inum(0) {}

	const JValue *get(const char *key) const
	{
		if (kind != JOBJECT)
		{
			return nullptr;
		}
		for (size_t i = 0; i < keys.size(); ++i)
		{
			if (keys[i] == key)
			{
				return &items[i];
			}
		}
		return nullptr;
	}
};

class JsonParser
{
public:
	explicit JsonParser(const std::string &text) : m_s(text.c_str()), m_p(text.c_str()) {}

	void parse(JValue &v)
	{
		value(v);
		ws();
		if (*m_p)
		{
			fail("trailing data");
		}
	}

private:
	void fail(const char *what)
	{
		int line = 1;
		for (const char *q = m_s; q < m_p; ++q)
		{
			if (*q == '\n')
			{
				++line;
			}
		}
		throw ConvertError(format("JSON line %d: %s", line, what));
	}

	static bool isDigit(char c)
	{
		return c >= '0' && c <= '9';
	}

	void digits()
	{
		while (isDigit(*m_p))
		{
			++m_p;
		}
	}

	void ws()
	{
		while (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')
		{
			++m_p;
		}
	}

	bool literal(const char *word)
	{
		size_t n = strlen(word);
		if (strncmp(m_p, word, n) == 0)
		{
			m_p += n;
			return true;
		}
		return false;
	}

	unsigned int hex4()
	{
		unsigned int v = 0;
		for (int i = 0; i < 4; ++i)
		{
			char c = *m_p++;
			v <<= 4;
			if (c >= '0' && c <= '9')
			{
				v |= c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				v |= c - 'a' + 10;
			}
			else if (c >= 'A' && c <= 'F')
			{
				v |= c - 'A' + 10;
			}
			else
			{
				fail("bad \\u escape");
			}
		}
		return v;
	}

	void string(std::string &out)
	{
		++m_p;
		while (*m_p != '"')
		{
			if (*m_p == 0)
			{
				fail("unterminated string");
			}
			if (*m_p != '\\')
			{
				out += *m_p++;
				continue;
			}
			++m_p;
			char c = *m_p++;
			switch (c)
			{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					unsigned int cp = hex4();
					if (cp >= 0xD800 && cp < 0xDC00 && m_p[0] == '\\' && m_p[1] == 'u')
					{
						m_p += 2;
						unsigned int lo = hex4();
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					}
					appendUtf8(out, cp);
					break;
				}
				default:
					fail("bad escape");
			}
		}
		++m_p;
	}

	void value(JValue &v)
	{
		ws();
		char c = *m_p;
		if (c == '{')
		{
			v.kind = JValue::JOBJECT;
			++m_p;
			ws();
			if (*m_p == '}')
			{
				++m_p;
				return;
			}
			for (;;)
			{
				ws();
				if (*m_p != '"')
				{
					fail("expected a key");
				}
				v.keys.push_back(std::string());
				string(v.keys.back());
				ws();
				if (*m_p++ != ':')
				{
					fail("expected ':'");
				}
				v.items.push_back(JValue());
				value(v.items.back());
				ws();
				if (*m_p == ',')
				{
					++m_p;
					continue;
				}
				if (*m_p++ != '}')
				{
					fail("expected ',' or '}'");
				}
				return;
			}
		}
		if (c == '[')
		{
			v.kind = JValue::JARRAY;
			++m_p;
			ws();
			if (*m_p == ']')
			{
				++m_p;
				return;
			}
			for (;;)
			{
				v.items.push_back(JValue());
				value(v.items.back());
				ws();
				if (*m_p == ',')
				{
					++m_p;
					continue;
				}
				if (*m_p++ != ']')
				{
					fail("expected ',' or ']'");
				}
				return;
			}
		}
		if (c == '"')
		{
			v.kind = JValue::JSTRING;
			string(v.str);
			return;
		}
		if (literal("true"))
		{
			v.kind = JValue::JBOOL;
			v.b = true;
			return;
		}
		if (literal("false"))
		{
			v.kind = JValue::JBOOL;
			v.b = false;
			return;
		}
		if (literal("null"))
		{
			v.kind = JValue::JNULL;
			return;
		}
		v.kind = JValue::JNUMBER;
		if (literal("NaN"))
		{
			v.num = strtod("nan", nullptr);
			return;
		}
		if (literal("Infinity"))
		{
			v.num = HUGE_VAL;
			return;
		}
		if (literal("-Infinity"))
		{
			v.num = -HUGE_VAL;
			return;
		}
		const char *start = m_p;
		if (*m_p == '-')
		{
			++m_p;
		}
		if (*m_p == '0')
		{
			++m_p;
		}
		else if (isDigit(*m_p))
		{
			digits();
		}
		else
		{
			fail(m_p == start ? "unexpected character" : "bad number");
		}
		bool isInt = true;
		if (*m_p == '.')
		{
			isInt = false;
			++m_p;
			if (!isDigit(*m_p))
			{
				fail("bad number");
			}
			digits();
		}
		if (*m_p == 'e' || *m_p == 'E')
		{
			isInt = false;
			++m_p;
			if (*m_p == '+' || *m_p == '-')
			{
				++m_p;
			}
			if (!isDigit(*m_p))
			{
				fail("bad number");
			}
			digits();
		}
		std::string token(start, m_p - start);
		v.isInt = isInt;
		v.num = strtod(token.c_str(), nullptr);
		v.inum = isInt ? _strtoi64(token.c_str(), nullptr, 10) : (long long)v.num;
	}

	const char *m_s;
	const char *m_p;
};

// ------------------------------------------------------------------------------------------------
// .scb writer

const JValue &member(const JValue &obj, const char *key)
{
	const JValue *v = obj.get(key);
	if (v == nullptr)
	{
		throw ConvertError(format("missing \"%s\"", key));
	}
	return *v;
}

long long asInt(const JValue &v, const char *what)
{
	if (v.kind == JValue::JBOOL)
	{
		return v.b ? 1 : 0;
	}
	if (v.kind != JValue::JNUMBER)
	{
		throw ConvertError(format("\"%s\" must be a number", what));
	}
	return v.isInt ? v.inum : (long long)v.num;
}

long long intOr(const JValue &obj, const char *key, long long fallback)
{
	const JValue *v = obj.get(key);
	return v ? asInt(*v, key) : fallback;
}

double realOr(const JValue &obj, const char *key, double fallback)
{
	const JValue *v = obj.get(key);
	if (v == nullptr)
	{
		return fallback;
	}
	if (v->kind != JValue::JNUMBER)
	{
		throw ConvertError(format("\"%s\" must be a number", key));
	}
	return v->isInt ? (double)v->inum : v->num;
}

bool boolOr(const JValue &obj, const char *key, bool fallback)
{
	const JValue *v = obj.get(key);
	return v ? asInt(*v, key) != 0 : fallback;
}

std::string stringOr(const JValue &obj, const char *key, const char *fallback)
{
	const JValue *v = obj.get(key);
	if (v == nullptr)
	{
		return fallback;
	}
	if (v->kind != JValue::JSTRING)
	{
		throw ConvertError(format("\"%s\" must be a string", key));
	}
	return v->str;
}

const std::vector<JValue> &arrayOf(const JValue &obj, const char *key)
{
	static const std::vector<JValue> empty;
	const JValue *v = obj.get(key);
	if (v == nullptr)
	{
		return empty;
	}
	if (v->kind != JValue::JARRAY)
	{
		throw ConvertError(format("\"%s\" must be a list", key));
	}
	return v->items;
}

class ScbOut
{
public:
	std::vector<std::pair<std::string, unsigned int> > table;

	void loadTable(const JValue *nameTable)
	{
		if (nameTable == nullptr)
		{
			return;
		}
		for (size_t i = 0; i < nameTable->items.size(); ++i)
		{
			const JValue &e = nameTable->items[i];
			if (e.kind != JValue::JARRAY || e.items.size() != 2 || e.items[0].kind != JValue::JSTRING)
			{
				throw ConvertError("\"nameTable\" entries must be [name, id]");
			}
			std::string name = utf8ToLatin1(e.items[0].str);
			unsigned int id = (unsigned int)asInt(e.items[1], "nameTable id");
			table.push_back(std::make_pair(name, id));
			m_ids[name] = id;
			if (id >= m_nextId)
			{
				m_nextId = id + 1;
			}
		}
	}

	ScbOut() : m_nextId(1) {}

	unsigned int id(const std::string &latin1)
	{
		std::map<std::string, unsigned int>::iterator it = m_ids.find(latin1);
		if (it != m_ids.end())
		{
			return it->second;
		}
		unsigned int nid = m_nextId++;
		m_ids[latin1] = nid;
		table.push_back(std::make_pair(latin1, nid));
		return nid;
	}

private:
	std::map<std::string, unsigned int> m_ids;
	unsigned int m_nextId;
};

void putI8(std::string &b, int v)
{
	b += (char)(signed char)v;
}

void putU16(std::string &b, unsigned int v)
{
	b += (char)(v & 0xFF);
	b += (char)((v >> 8) & 0xFF);
}

void putI32(std::string &b, long long v)
{
	int i = (int)v;
	b.append((const char *)&i, 4);
}

void putF32(std::string &b, double v)
{
	float f = (float)v;
	b.append((const char *)&f, 4);
}

void putAscii(std::string &b, const std::string &utf8)
{
	std::string s = utf8ToLatin1(utf8);
	if (s.size() > 0xFFFF)
	{
		throw ConvertError("string longer than 65535 characters");
	}
	putU16(b, (unsigned int)s.size());
	b += s;
}

void putUnicode(std::string &b, const std::string &utf8)
{
	std::vector<unsigned int> cps = decodeUtf8(utf8);
	std::string units;
	for (size_t i = 0; i < cps.size(); ++i)
	{
		unsigned int cp = cps[i];
		if (cp >= 0x10000)
		{
			cp -= 0x10000;
			putU16(units, 0xD800 + (cp >> 10));
			putU16(units, 0xDC00 + (cp & 0x3FF));
		}
		else
		{
			putU16(units, cp);
		}
	}
	putU16(b, (unsigned int)(units.size() / 2));
	b += units;
}

std::string chunk(ScbOut &w, const std::string &label, long long ver, const std::string &body)
{
	std::string out;
	putI32(out, w.id(utf8ToLatin1(label)));
	putU16(out, (unsigned int)ver);
	putI32(out, (long long)body.size());
	return out + body;
}

std::string writeRaw(ScbOut &w, const JValue &item)
{
	return chunk(w, stringOr(item, "chunk", ""), intOr(item, "version", 0), base64Decode(stringOr(item, "raw", "")));
}

std::string writeDict(ScbOut &w, const JValue &d)
{
	if (d.kind != JValue::JOBJECT)
	{
		throw ConvertError("a dict must be an object");
	}
	std::string b;
	putU16(b, (unsigned int)d.keys.size());
	for (size_t i = 0; i < d.keys.size(); ++i)
	{
		const JValue &v = d.items[i];
		long long key = (long long)w.id(utf8ToLatin1(d.keys[i])) << 8;
		if (v.kind == JValue::JBOOL)
		{
			putI32(b, key | DICT_BOOL);
			putI8(b, v.b ? 1 : 0);
		}
		else if (v.kind == JValue::JNUMBER && v.isInt)
		{
			putI32(b, key | DICT_INT);
			putI32(b, v.inum);
		}
		else if (v.kind == JValue::JNUMBER)
		{
			putI32(b, key | DICT_REAL);
			putF32(b, v.num);
		}
		else if (v.kind == JValue::JOBJECT && v.get("unicode") != nullptr)
		{
			putI32(b, key | DICT_UNICODESTRING);
			putUnicode(b, stringOr(v, "unicode", ""));
		}
		else if (v.kind == JValue::JSTRING)
		{
			putI32(b, key | DICT_ASCIISTRING);
			putAscii(b, v.str);
		}
		else
		{
			throw ConvertError(format("dict key %s has a value of unsupported type", d.keys[i].c_str()));
		}
	}
	return b;
}

int indexOf(const char *const *names, int count, const std::string &name, const char *what)
{
	for (int i = 0; i < count; ++i)
	{
		if (name == names[i])
		{
			return i;
		}
	}
	throw ConvertError(format("unknown %s \"%s\"", what, name.c_str()));
}

std::string writeParameter(const JValue &p)
{
	const JValue &tv = member(p, "type");
	int t = tv.kind == JValue::JSTRING ? indexOf(PARAMETER_TYPES, NUM_PARAMETER_TYPES, tv.str, "parameter type") : (int)asInt(tv, "type");
	std::string b;
	putI32(b, t);
	if (t == PARAM_COORD3D)
	{
		const std::vector<JValue> &c = arrayOf(p, "coord");
		if (c.size() != 3)
		{
			throw ConvertError("\"coord\" needs three numbers");
		}
		for (int k = 0; k < 3; ++k)
		{
			putF32(b, c[k].isInt ? (double)c[k].inum : c[k].num);
		}
		return b;
	}
	long long value = intOr(p, "int", 0);
	if (t == PARAM_COMPARISON && p.get("comparison") != nullptr)
	{
		value = indexOf(COMPARISONS, NUM_COMPARISONS, stringOr(p, "comparison", ""), "comparison");
	}
	putI32(b, value);
	putF32(b, realOr(p, "real", 0.0));
	putAscii(b, stringOr(p, "string", ""));
	return b;
}

typedef std::map<std::string, long long> TypeMap;

std::string writeConditionOrAction(ScbOut &w, const char *label, const JValue &item, int keyVersion, const TypeMap &types, int defaultVersion)
{
	long long ver = intOr(item, "version", defaultVersion);
	const JValue *name = item.get("name");
	long long t;
	if (item.get("type") != nullptr)
	{
		t = intOr(item, "type", 0);
	}
	else
	{
		TypeMap::const_iterator it = name ? types.find(name->str) : types.end();
		t = it == types.end() ? 0 : it->second;
	}
	std::string b;
	putI32(b, t);
	if (ver >= keyVersion)
	{
		if (name == nullptr || name->kind != JValue::JSTRING)
		{
			throw ConvertError(format("%s of type %d needs a \"name\" for version %d", label, (int)t, (int)ver));
		}
		putI32(b, ((long long)w.id(utf8ToLatin1(name->str)) << 8) | DICT_ASCIISTRING);
	}
	const std::vector<JValue> &params = arrayOf(item, "parameters");
	putI32(b, (long long)params.size());
	for (size_t i = 0; i < params.size(); ++i)
	{
		b += writeParameter(params[i]);
	}
	return chunk(w, label, ver, b);
}

struct Types
{
	TypeMap conditions;
	TypeMap actions;
};

std::string writeScript(ScbOut &w, const JValue &s, const Types &types)
{
	long long ver = intOr(s, "version", SCRIPT_VERSION);
	std::string b;
	putAscii(b, stringOr(s, "name", ""));
	putAscii(b, stringOr(s, "comment", ""));
	putAscii(b, stringOr(s, "conditionComment", ""));
	putAscii(b, stringOr(s, "actionComment", ""));
	putI8(b, boolOr(s, "active", true));
	putI8(b, boolOr(s, "oneShot", false));
	putI8(b, boolOr(s, "easy", true));
	putI8(b, boolOr(s, "normal", true));
	putI8(b, boolOr(s, "hard", true));
	putI8(b, boolOr(s, "subroutine", false));
	if (ver >= 2)
	{
		putI32(b, intOr(s, "delayEvaluationSeconds", 0));
	}
	const std::vector<JValue> &ors = arrayOf(s, "or");
	for (size_t i = 0; i < ors.size(); ++i)
	{
		std::string conds;
		for (size_t k = 0; k < ors[i].items.size(); ++k)
		{
			conds += writeConditionOrAction(w, "Condition", ors[i].items[k], CONDITION_KEY_VERSION, types.conditions, CONDITION_VERSION);
		}
		b += chunk(w, "OrCondition", OR_VERSION, conds);
	}
	const std::vector<JValue> &actions = arrayOf(s, "actions");
	for (size_t i = 0; i < actions.size(); ++i)
	{
		b += writeConditionOrAction(w, "ScriptAction", actions[i], ACTION_KEY_VERSION, types.actions, ACTION_VERSION);
	}
	const std::vector<JValue> &falseActions = arrayOf(s, "falseActions");
	for (size_t i = 0; i < falseActions.size(); ++i)
	{
		b += writeConditionOrAction(w, "ScriptActionFalse", falseActions[i], ACTION_KEY_VERSION, types.actions, ACTION_VERSION);
	}
	const std::vector<JValue> &unknown = arrayOf(s, "unknown");
	for (size_t i = 0; i < unknown.size(); ++i)
	{
		b += writeRaw(w, unknown[i]);
	}
	return chunk(w, "Script", ver, b);
}

std::string writeScriptList(ScbOut &w, const JValue &sl, const Types &types)
{
	std::string b;
	const std::vector<JValue> &scripts = arrayOf(sl, "scripts");
	for (size_t i = 0; i < scripts.size(); ++i)
	{
		b += writeScript(w, scripts[i], types);
	}
	const std::vector<JValue> &groups = arrayOf(sl, "groups");
	for (size_t i = 0; i < groups.size(); ++i)
	{
		const JValue &g = groups[i];
		long long ver = intOr(g, "version", GROUP_VERSION);
		std::string gb;
		putAscii(gb, stringOr(g, "name", ""));
		putI8(gb, boolOr(g, "active", true));
		if (ver >= 2)
		{
			putI8(gb, boolOr(g, "subroutine", false));
		}
		const std::vector<JValue> &gs = arrayOf(g, "scripts");
		for (size_t k = 0; k < gs.size(); ++k)
		{
			gb += writeScript(w, gs[k], types);
		}
		b += chunk(w, "ScriptGroup", ver, gb);
	}
	const std::vector<JValue> &unknown = arrayOf(sl, "unknown");
	for (size_t i = 0; i < unknown.size(); ++i)
	{
		b += writeRaw(w, unknown[i]);
	}
	return chunk(w, "ScriptList", intOr(sl, "version", LIST_VERSION), b);
}

std::string writeTopChunk(ScbOut &w, const JValue &c, const Types &types)
{
	if (c.get("raw") != nullptr)
	{
		return writeRaw(w, c);
	}
	std::string label = stringOr(c, "chunk", "");
	long long ver = intOr(c, "version", 0);
	std::string b;
	if (label == "PlayerScriptsList")
	{
		const std::vector<JValue> &lists = arrayOf(c, "scriptLists");
		for (size_t i = 0; i < lists.size(); ++i)
		{
			b += writeScriptList(w, lists[i], types);
		}
	}
	else if (label == "ScriptsPlayers")
	{
		bool exportSides = boolOr(c, "exportSides", false);
		const std::vector<JValue> &players = arrayOf(c, "players");
		putI32(b, exportSides ? 1 : 0);
		putI32(b, (long long)players.size());
		for (size_t i = 0; i < players.size(); ++i)
		{
			putAscii(b, stringOr(players[i], "name", ""));
			if (exportSides)
			{
				const JValue *d = players[i].get("dict");
				JValue empty;
				empty.kind = JValue::JOBJECT;
				b += writeDict(w, d ? *d : empty);
			}
		}
	}
	else if (label == "ObjectsList")
	{
		const std::vector<JValue> &objects = arrayOf(c, "objects");
		for (size_t i = 0; i < objects.size(); ++i)
		{
			const JValue &o = objects[i];
			if (o.get("raw") != nullptr)
			{
				b += writeRaw(w, o);
				continue;
			}
			const std::vector<JValue> &loc = arrayOf(o, "location");
			if (loc.size() != 3)
			{
				throw ConvertError("\"location\" needs three numbers");
			}
			std::string ob;
			for (int k = 0; k < 3; ++k)
			{
				putF32(ob, loc[k].isInt ? (double)loc[k].inum : loc[k].num);
			}
			putF32(ob, realOr(o, "angle", 0.0));
			putI32(ob, intOr(o, "flags", 0));
			putAscii(ob, stringOr(o, "name", ""));
			ob += writeDict(w, member(o, "properties"));
			b += chunk(w, "Object", intOr(o, "version", OBJECT_VERSION), ob);
		}
	}
	else if (label == "PolygonTriggers")
	{
		const std::vector<JValue> &triggers = arrayOf(c, "triggers");
		putI32(b, (long long)triggers.size());
		for (size_t i = 0; i < triggers.size(); ++i)
		{
			const JValue &t = triggers[i];
			putAscii(b, stringOr(t, "name", ""));
			putI32(b, intOr(t, "id", 0));
			putI8(b, boolOr(t, "water", false));
			putI8(b, boolOr(t, "river", false));
			putI32(b, intOr(t, "riverStart", 0));
			const std::vector<JValue> &points = arrayOf(t, "points");
			putI32(b, (long long)points.size());
			for (size_t k = 0; k < points.size(); ++k)
			{
				if (points[k].items.size() != 3)
				{
					throw ConvertError("a trigger point needs three numbers");
				}
				for (int n = 0; n < 3; ++n)
				{
					putI32(b, asInt(points[k].items[n], "point"));
				}
			}
		}
	}
	else if (label == "ScriptTeams")
	{
		const std::vector<JValue> &teams = arrayOf(c, "teams");
		for (size_t i = 0; i < teams.size(); ++i)
		{
			b += writeDict(w, teams[i]);
		}
	}
	else if (label == "WaypointsList")
	{
		const std::vector<JValue> &links = arrayOf(c, "links");
		putI32(b, (long long)links.size());
		for (size_t i = 0; i < links.size(); ++i)
		{
			if (links[i].items.size() != 2)
			{
				throw ConvertError("a waypoint link needs two ids");
			}
			putI32(b, asInt(links[i].items[0], "link"));
			putI32(b, asInt(links[i].items[1], "link"));
		}
	}
	else
	{
		throw ConvertError("chunk " + label + " has no \"raw\" data and no known layout");
	}
	return chunk(w, label, ver, b);
}

void collectTypes(const std::vector<JValue> &scripts, Types &types)
{
	for (size_t i = 0; i < scripts.size(); ++i)
	{
		const JValue &s = scripts[i];
		const std::vector<JValue> &ors = arrayOf(s, "or");
		for (size_t k = 0; k < ors.size(); ++k)
		{
			for (size_t n = 0; n < ors[k].items.size(); ++n)
			{
				const JValue &c = ors[k].items[n];
				if (c.get("name") != nullptr && c.get("type") != nullptr && types.conditions.find(c.get("name")->str) == types.conditions.end())
				{
					types.conditions[c.get("name")->str] = intOr(c, "type", 0);
				}
			}
		}
		const char *const lists[] = { "actions", "falseActions" };
		for (int l = 0; l < 2; ++l)
		{
			const std::vector<JValue> &actions = arrayOf(s, lists[l]);
			for (size_t k = 0; k < actions.size(); ++k)
			{
				const JValue &a = actions[k];
				if (a.get("name") != nullptr && a.get("type") != nullptr && types.actions.find(a.get("name")->str) == types.actions.end())
				{
					types.actions[a.get("name")->str] = intOr(a, "type", 0);
				}
			}
		}
	}
}

std::string tempScbPath()
{
	char dir[MAX_PATH];
	char path[MAX_PATH];
	if (::GetTempPathA(MAX_PATH, dir) == 0 || ::GetTempFileNameA(dir, "scb", 0, path) == 0)
	{
		return std::string();
	}
	return path;
}

bool readFile(const char *path, std::string &out)
{
	FILE *f = fopen(path, "rb");
	if (f == nullptr)
	{
		return false;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	try
	{
		out.resize(n > 0 ? n : 0);
	}
	catch (...)
	{
		fclose(f);
		return false;
	}
	bool ok = n <= 0 || fread(&out[0], 1, n, f) == (size_t)n;
	fclose(f);
	return ok;
}

bool writeFile(const char *path, const std::string &data)
{
	FILE *f = fopen(path, "wb");
	if (f == nullptr)
	{
		return false;
	}
	bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
	ok = fclose(f) == 0 && ok;
	return ok;
}

} // namespace

// ------------------------------------------------------------------------------------------------

bool ScriptJson_ScbToJson(const std::string &scb, std::string &json, std::string &error)
{
	if (scb.size() < 8 || scb.compare(0, 4, "CkMp") != 0)
	{
		error = "not a chunk file (no CkMp header)";
		return false;
	}
	try
	{
		NameMap names;
		std::vector<std::pair<std::string, unsigned int> > table;
		int count;
		memcpy(&count, scb.data() + 4, 4);
		size_t p = 8;
		for (int i = 0; i < count; ++i)
		{
			if (p >= scb.size())
			{
				throw ConvertError("name table runs past the end of the file");
			}
			unsigned int n = (unsigned char)scb[p];
			if (p + 5 + n > scb.size())
			{
				throw ConvertError("name table runs past the end of the file");
			}
			unsigned int id;
			memcpy(&id, scb.data() + p + 1 + n, 4);
			std::string name = latin1ToUtf8(scb.data() + p + 1, n);
			table.push_back(std::make_pair(name, id));
			names[id] = name;
			p += 5 + n;
		}

		JsonOut o;
		o.open('{');
		o.key("chunks");
		o.open('[');
		Reader top(scb, names, p, scb.size());
		ChunkIter it(top);
		while (it.next())
		{
			size_t pos = o.mark();
			size_t depth = o.depth();
			int itemCount = o.count();
			o.item();
			size_t bodyStart = it.body->p;
			bool decoded = false;
			try
			{
				decoded = emitTopChunk(o, it.label, it.ver, *it.body);
			}
			catch (const ConvertError &e)
			{
				OutputDebugStringA((it.label + ": " + e.message + ", kept raw\n").c_str());
			}
			if (!decoded)
			{
				o.rewind(pos, depth, itemCount);
				o.item();
				it.body->p = bodyStart;
				emitRaw(o, it.label, it.ver, *it.body);
			}
		}
		o.close(']');
		o.key("nameTable");
		o.open('[');
		for (size_t i = 0; i < table.size(); ++i)
		{
			o.item();
			o.open('[');
			o.item();
			o.string(table[i].first);
			o.item();
			o.integer(table[i].second);
			o.close(']');
		}
		o.close(']');
		o.close('}');
		json.swap(o.text);
		json += '\n';
		return true;
	}
	catch (const ConvertError &e)
	{
		error = e.message;
		return false;
	}
	catch (const std::bad_alloc &)
	{
		error = "Out of memory while converting the script file";
		return false;
	}
	catch (...)
	{
		error = "Unexpected error while converting the script file";
		return false;
	}
}

bool ScriptJson_JsonToScb(const std::string &json, std::string &scb, std::string &error)
{
	try
	{
		JValue root;
		JsonParser(json).parse(root);
		if (root.kind != JValue::JOBJECT)
		{
			throw ConvertError("the file must hold a JSON object");
		}

		Types types;
		const std::vector<JValue> &chunks = arrayOf(root, "chunks");
		for (size_t i = 0; i < chunks.size(); ++i)
		{
			const std::vector<JValue> &lists = arrayOf(chunks[i], "scriptLists");
			for (size_t k = 0; k < lists.size(); ++k)
			{
				collectTypes(arrayOf(lists[k], "scripts"), types);
				const std::vector<JValue> &groups = arrayOf(lists[k], "groups");
				for (size_t g = 0; g < groups.size(); ++g)
				{
					collectTypes(arrayOf(groups[g], "scripts"), types);
				}
			}
		}

		ScbOut w;
		w.loadTable(root.get("nameTable"));
		std::string body;
		for (size_t i = 0; i < chunks.size(); ++i)
		{
			body += writeTopChunk(w, chunks[i], types);
		}

		scb = "CkMp";
		putI32(scb, (long long)w.table.size());
		for (size_t i = 0; i < w.table.size(); ++i)
		{
			const std::string &name = w.table[i].first;
			if (name.size() > 255)
			{
				throw ConvertError("name longer than 255 characters: " + name.substr(0, 60));
			}
			scb += (char)(unsigned char)name.size();
			scb += name;
			putI32(scb, w.table[i].second);
		}
		scb += body;
		return true;
	}
	catch (const ConvertError &e)
	{
		error = e.message;
		return false;
	}
	catch (const std::bad_alloc &)
	{
		error = "Out of memory while converting the script file";
		return false;
	}
	catch (...)
	{
		error = "Unexpected error while converting the script file";
		return false;
	}
}

bool ScriptJson_IsJsonPath(const char *path)
{
	size_t n = path ? strlen(path) : 0;
	return n >= 5 && _stricmp(path + n - 5, ".json") == 0;
}

// ------------------------------------------------------------------------------------------------

ScriptJsonImportFile::ScriptJsonImportFile()
{
}

ScriptJsonImportFile::~ScriptJsonImportFile()
{
	if (!m_tempPath.empty())
	{
		::DeleteFileA(m_tempPath.c_str());
	}
}

bool ScriptJsonImportFile::open(const char *path)
{
	if (!ScriptJson_IsJsonPath(path))
	{
		m_scbPath = path;
		return true;
	}
	std::string json;
	std::string scb;
	if (!readFile(path, json))
	{
		m_error = std::string("Could not read ") + path;
		return false;
	}
	if (!ScriptJson_JsonToScb(json, scb, m_error))
	{
		m_error = std::string(path) + "\n\n" + m_error;
		return false;
	}
	m_tempPath = tempScbPath();
	if (m_tempPath.empty() || !writeFile(m_tempPath.c_str(), scb))
	{
		m_error = "Could not write a temporary .scb file";
		return false;
	}
	m_scbPath = m_tempPath;
	return true;
}

ScriptJsonExportFile::ScriptJsonExportFile(const char *path) : m_targetPath(path)
{
	if (!ScriptJson_IsJsonPath(path))
	{
		m_scbPath = path;
		return;
	}
	m_tempPath = tempScbPath();
	m_scbPath = m_tempPath;
	if (m_tempPath.empty())
	{
		m_error = "Could not create a temporary .scb file";
	}
}

ScriptJsonExportFile::~ScriptJsonExportFile()
{
	if (!m_tempPath.empty())
	{
		::DeleteFileA(m_tempPath.c_str());
	}
}

bool ScriptJsonExportFile::finish()
{
	if (m_tempPath.empty())
	{
		return m_error.empty();
	}
	std::string scb;
	std::string json;
	if (!readFile(m_tempPath.c_str(), scb))
	{
		m_error = "Could not read the temporary .scb file";
		return false;
	}
	if (!ScriptJson_ScbToJson(scb, json, m_error))
	{
		return false;
	}
	if (!writeFile(m_targetPath.c_str(), json))
	{
		m_error = "Could not write " + m_targetPath;
		return false;
	}
	return true;
}

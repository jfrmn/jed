#include "color.hh"
#include "basic.hh"
#include "logging.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 1
#include <toml++/toml.hpp>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d2d1.h>

const Color COLOR_TRANSPARENT {0.0f, 0.0f, 0.0f, 0.0f};
const Color COLOR_RED         {1.0f, 0.0f, 0.0f, 1.0f};
const Color COLOR_GREEN       {0.0f, 1.0f, 0.0f, 1.0f};
const Color COLOR_BLUE        {0.0f, 0.0f, 1.0f, 1.0f};
const Color COLOR_WHITE       {1.0f, 1.0f, 1.0f, 1.0f};
const Color COLOR_BLACK       {0.0f, 0.0f, 0.0f, 1.0f};

Color Color::FromKnown(int d2dIndex) {
	const auto enumVal = static_cast<D2D1::ColorF::Enum>(d2dIndex);
	const D2D_COLOR_F d2dColor = D2D1::ColorF(enumVal);
	return Color {d2dColor.r, d2dColor.g, d2dColor.b, d2dColor.a};
}

bool Color::FromToml(const toml::node& node, /*out*/ Color* color) {
	if (node.is_array()) {
		const auto arr = node.as<toml::array>();
		ASSERT(arr);
		
		if (arr->size() < 3) LogWarning("%s: insufficient number of values (expected 3 or 4)", Str(arr->source()));
		if (arr->size() > 4) LogWarning("%s: too many number of values (expected 3 or 4)", Str(arr->source()));
		if (arr->empty()) return false;
				
		for (u64 i = 0u; i < std::min(4llu, arr->size()); i++) {
			auto nodeValue = arr->get(i);
			if (!nodeValue) {
				color->array[i] = 0.0f;
			} else if (nodeValue->is_integer()) {
				const s64 asInt = nodeValue->as_integer()->get();
				color->array[i] = asInt / 255.0f;
			} else if (nodeValue->is_floating_point()) {
				color->array[i] = static_cast<f32>(nodeValue->as_floating_point()->get());
			} else {
				LogWarning("%s: invalid value type (should be int of float)", Str(nodeValue->source()));
				color->array[i] = 0.0f;
			}
		}
	
	} else if (node.is_table()) {
		const auto tbl = node.as<toml::table>();
		ASSERT(tbl);
	
		f32* channels[] {&color->r, &color->g, &color->b, &color->a};
		const char channelNames[] {'r', 'g', 'b', 'a'};
		
		for (u64 i = 0u; i < 4; i++) {
			const std::string_view channelName {&channelNames[i], 1u};
			const toml::node* nodeValue = tbl->get(channelName);
			
			if (!nodeValue) {
				if (channelNames[i] != 'a')
					LogWarning("%s: missing entry '%.*s'", Str(tbl->source()), (int)channelName.size(), channelName.data());
				*channels[i] = 0.0f;
			} else if (nodeValue->is_integer()) {
				const s64 asInt = nodeValue->as_integer()->get();
				*channels[i] = asInt / 255.0f;
			} else if (nodeValue->is_floating_point()) {
				*channels[i] = static_cast<f32>(nodeValue->as_floating_point()->get());
			} else {
				LogWarning("%s: invalid value type (should be int of float)", Str(tbl->source()));
				*channels[i] = 0.0f;
			}
		}
	
	} else if (node.is_string()) {
		// @TODO support more colors
		const std::string_view clrName = node.as_string()->get();
		if      (clrName == "red") *color = Color {1.0f, 0.0f, 0.0f, 1.0f};
		else if (clrName == "green") *color = Color {0.0f, 1.0f, 0.0f, 1.0f};
		else if (clrName == "blue") *color = Color {0.0f, 0.0f, 1.0f, 1.0f};
		else if (clrName == "yellow") *color = Color {1.0f, 1.0f, 0.0f, 1.0f};
		else if (clrName == "magenta") *color = Color {1.0f, 0.0f, 1.0f, 1.0f};
		else if (clrName == "cyan") *color = Color {0.0f, 1.0f, 1.0f, 1.0f};
		else if (clrName == "black") *color = Color {0.0f, 0.0f, 0.0f, 1.0f};
		else if (clrName == "white") *color = Color {1.0f, 1.0f, 1.0f, 1.0f};
		else if (clrName == "transparent") *color = Color {0.0f, 0.0f, 0.0f, 0.0f};
		else {
			LogError("%s: unknown named color", Str(node.source()));
			return false;
		}
	
	} else {
		LogError("%s: expected a table, array or string", Str(node.source()));
		return false;
	}
	
	return true;
}

_D3DCOLORVALUE Color::ToD2D() const {
	return D2D_COLOR_F {
		.r = this->r,
		.g = this->g,
		.b = this->b,
		.a = this->a};
}

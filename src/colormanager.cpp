#include "colormanager.h"

#include "config.h"
#include "confighandlerexception.h"
#include "feedlistformaction.h"
#include "filebrowserformaction.h"
#include "helpformaction.h"
#include "itemlistformaction.h"
#include "itemviewformaction.h"
#include "logger.h"
#include "matcherexception.h"
#include "pbview.h"
#include "selectformaction.h"
#include "strprintf.h"
#include "urlviewformaction.h"
#include "utils.h"

using namespace podboat;

namespace newsboat {

ColorManager::ColorManager()
{
}

ColorManager::~ColorManager() {}

void ColorManager::register_commands(ConfigParser& cfgparser)
{
	cfgparser.register_handler("color", *this);
}

void ColorManager::handle_action(const std::string& action,
	const std::vector<std::string>& params)
{
	LOG(Level::DEBUG,
		"ColorManager::handle_action(%s,...) was called",
		action);
	if (action == "color") {
		if (params.size() < 3) {
			throw ConfigHandlerException(ActionHandlerStatus::TOO_FEW_PARAMS);
		}

		/*
		 * the command syntax is:
		 * color <element> <fgcolor> <bgcolor> [<attribute> ...]
		 */
		const std::string element = params[0];
		const std::string fgcolor = params[1];
		const std::string bgcolor = params[2];

		if (!utils::is_valid_color(fgcolor)) {
			throw ConfigHandlerException(strprintf::fmt(
					_("`%s' is not a valid color"), fgcolor));
		}
		if (!utils::is_valid_color(bgcolor)) {
			throw ConfigHandlerException(strprintf::fmt(
					_("`%s' is not a valid color"), bgcolor));
		}

		const std::vector<std::string> attribs(
			std::next(params.cbegin(), 3),
			params.cend());
		for (const auto& attr : attribs) {
			if (!utils::is_valid_attribute(attr)) {
				throw ConfigHandlerException(strprintf::fmt(
						_("`%s' is not a valid attribute"),
						attr));
			}
		}

		/* we only allow certain elements to be configured, also to
		 * indicate the user possible mis-spellings */
		const std::vector<std::string> supported_elements({
			"listnormal",
			"listfocus",
			"listnormal_unread",
			"listfocus_unread",
			"info",
			"background",
			"article",
			"end-of-text-marker",
			"title"
		});
		const auto element_is_supported = std::find(supported_elements.cbegin(),
				supported_elements.cend(), element) != supported_elements.cend();

		if (element_is_supported) {
			element_styles[element] = {fgcolor, bgcolor, attribs};
		} else {
			throw ConfigHandlerException(strprintf::fmt(
					_("`%s' is not a valid configuration element"),
					element));
		}
	} else {
		throw ConfigHandlerException(ActionHandlerStatus::INVALID_COMMAND);
	}
}

void ColorManager::dump_config(std::vector<std::string>& config_output) const
{
	for (const auto& element_style : element_styles) {
		const std::string& element = element_style.first;
		const TextStyle& style = element_style.second;
		std::string configline = strprintf::fmt("color %s %s %s",
				element,
				style.fg_color,
				style.bg_color);
		for (const auto& attrib : style.attributes) {
			configline.append(" ");
			configline.append(attrib);
		}
		config_output.push_back(configline);
	}
}

std::string format_style(const TextStyle& style)
{
	std::string result;

	if (style.fg_color != "default") {
		result.append("fg=");
		result.append(style.fg_color);
	}
	if (style.bg_color != "default") {
		if (!result.empty()) {
			result.append(",");
		}
		result.append("bg=");
		result.append(style.bg_color);
	}
	for (const auto& attr : style.attributes) {
		if (!result.empty()) {
			result.append(",");
		}
		result.append("attr=");
		result.append(attr);
	}

	return result;
}

void ColorManager::apply_colors(
	std::function<void(const std::string&, const std::string&)> stfl_value_setter)
const
{
	for (const auto& element_style : element_styles) {
		const std::string& element = element_style.first;
		const TextStyle& style = element_style.second;
		const auto colorattr = format_style(style);

		LOG(Level::DEBUG,
			"ColorManager::apply_colors: %s %s\n",
			element,
			colorattr);

		stfl_value_setter(element, colorattr);

		if (element == "article") {
			std::string bold = colorattr;
			std::string underline = colorattr;
			if (!bold.empty()) {
				bold.append(",");
			}
			if (!underline.empty()) {
				underline.append(",");
			}
			bold.append("attr=bold");
			underline.append("attr=underline");
			// STFL will just ignore those in forms which don't have the
			// `color_bold` and `color_underline` variables.
			LOG(Level::DEBUG, "ColorManager::apply_colors: color_bold %s\n", bold);
			stfl_value_setter("color_bold", bold);
			LOG(Level::DEBUG, "ColorManager::apply_colors: color_underline %s\n", underline);
			stfl_value_setter("color_underline", underline);
		}
	}

	const auto title_style = element_styles.find("title");
	const auto info_style = element_styles.find("info");
	if (title_style == element_styles.cend() && info_style != element_styles.cend()) {
		// `title` falls back to `info` when available
		const auto style = format_style(info_style->second);
		LOG(Level::DEBUG, "ColorManager::apply_colors: title inherited from info %s\n", style);
		stfl_value_setter("title", style);
	}
}

} // namespace newsboat

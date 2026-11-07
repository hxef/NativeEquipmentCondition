#include "UI/PerkChart.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/PerkCard.h"

#include <array>

namespace PerkChart
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Called by the chart every frame. The chart sets the card's text from
		// its own script on every move of the mouse and every rank key, and a
		// field that sizes itself grows at each of those sets, so the field is
		// set up once and only checked after that. A field another mod already
		// sizes is left as it is.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				auto field = a_params.movie ? Roles::PerkCard::Description(*a_params.movie) : std::nullopt;
				if (!field || Flash::String(*field, "autoSize"sv) != "none"sv) {
					return;
				}

				// Centre keeps the field's top and its width, and only its
				// height follows the lines, since the field wraps its words.
				if (Flash::Set(*field, "autoSize"sv, Value("center"))) {
					TraceLog::Line("menu", "LevelUpMenu.swf: the perk card's description sizes itself to its text, so every line shows");
				}
			}
		};

		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!MenuMovies::IsMovie(a_file, "LevelUpMenu.swf"sv)) {
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The perk chart has no stage to listen on, so a long perk description shows its first 2 lines there.");
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The perk chart refused the frame listener, so a long perk description shows its first 2 lines there.");
		}
	}
}

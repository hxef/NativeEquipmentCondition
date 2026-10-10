#include "UI/Repair/RepairPrompt.h"

#include "Core/Text/Text.h"
#include "UI/MessageBox.h"

#include <utility>

namespace RepairPrompt
{
	namespace
	{
		// Both the game's own words, looked up by the game.
		constexpr const char* TITLE = "$REPAIR";
		constexpr const char* CANCEL = "$CANCEL";

		// What the box calls back into, see MessageBox::Callback.
		class Answered : public MessageBox::Callback
		{
		public:
			Answered(std::size_t a_offers, std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled) :
				offers(a_offers), chosen(std::move(a_chosen)), cancelled(std::move(a_cancelled))
			{}

			void operator()(std::uint8_t a_button) override
			{
				// Any button after the offers means cancel. Without the task
				// queue the box counts as cancelled at once, which spends
				// nothing and opens no menu.
				const auto  index = static_cast<std::size_t>(a_button);
				const auto* tasks = F4SE::GetTaskInterface();
				if (!tasks) {
					if (cancelled) {
						cancelled();
					}
					return;
				}
				if (index >= offers) {
					if (cancelled) {
						tasks->AddTask(cancelled);
					}
					return;
				}
				tasks->AddTask([run = chosen, index] { run(index); });
			}

			F4_HEAP_REDEFINE_NEW(Answered);

			std::size_t                      offers;
			std::function<void(std::size_t)> chosen;
			std::function<void()>            cancelled;
		};
	}

	void Ask(std::string_view a_over, std::string_view a_name, std::uint32_t a_percent,
		std::vector<std::string> a_buttons,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled)
	{
		std::string body;
		if (!a_over.empty()) {
			body += a_over;
			body += "\n\n";
		}
		body += Text::RepairQuestion(a_name, a_percent);

		const auto offers = a_buttons.size();
		a_buttons.emplace_back(CANCEL);
		MessageBox::Ask(TITLE, body.c_str(), a_buttons,
			new Answered(offers, std::move(a_chosen), std::move(a_cancelled)));
	}
}

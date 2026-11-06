#include "UI/Repair/RepairPrompt.h"

#include "Core/Text/Text.h"
#include "UI/MessageBox.h"

#include <algorithm>
#include <utility>

namespace RepairPrompt
{
	namespace
	{
		// Both vanilla words, looked up by the game.
		constexpr const char* TITLE = "$REPAIR";
		constexpr const char* CANCEL = "$CANCEL";

		// What the box calls back into, see MessageBox::Callback.
		class Answered : public MessageBox::Callback
		{
		public:
			Answered(std::size_t a_rows, std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled) :
				rows(a_rows), chosen(std::move(a_chosen)), cancelled(std::move(a_cancelled))
			{}

			void operator()(std::uint8_t a_button) override
			{
				// Any button after the rows means cancel. Without the task
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
				if (index >= rows) {
					if (cancelled) {
						tasks->AddTask(cancelled);
					}
					return;
				}
				tasks->AddTask([run = chosen, index] { run(index); });
			}

			F4_HEAP_REDEFINE_NEW(Answered);

			std::size_t                      rows;
			std::function<void(std::size_t)> chosen;
			std::function<void()>            cancelled;
		};
	}

	void Ask(std::string_view a_over, std::string_view a_name, std::uint32_t a_percent,
		std::vector<std::string> a_buttons,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled,
		std::optional<Top> a_top, Start a_start)
	{
		// The box lists the offers the other way round, below the top row.
		const auto offers = a_buttons.size();
		const auto above = a_top ? std::size_t{ 1 } : std::size_t{ 0 };
		std::ranges::reverse(a_buttons);
		if (a_top) {
			a_buttons.insert(a_buttons.begin(), a_top->label);
		}
		// The highest offer sits right below the top row.
		auto start = static_cast<std::uint32_t>(above);
		switch (a_start) {
		case Start::kNone:
			start = MessageBox::NO_ROW;
			break;
		case Start::kTop:
			start = 0;
			break;
		case Start::kLowest:
			start = static_cast<std::uint32_t>(above + std::max<std::size_t>(offers, 1) - 1);
			break;
		}
		AskRows(a_over, Text::RepairQuestion(a_name, a_percent), std::move(a_buttons),
			[offers, above, chosen = std::move(a_chosen), top = a_top ? std::move(a_top->chosen) : std::function<void()>{}](std::size_t a_row) {
				if (a_row < above) {
					top();
				} else {
					chosen(offers - 1 - (a_row - above));
				}
			},
			std::move(a_cancelled), start);
	}

	void AskRows(std::string_view a_over, std::string_view a_question, std::vector<std::string> a_rows,
		std::function<void(std::size_t)> a_chosen, std::function<void()> a_cancelled, std::uint32_t a_start)
	{
		std::string body;
		if (!a_over.empty()) {
			body += a_over;
			body += "\n\n";
		}
		body += a_question;

		const auto rows = a_rows.size();
		a_rows.emplace_back(CANCEL);
		MessageBox::Ask(TITLE, body.c_str(), a_rows, new Answered(rows, std::move(a_chosen), std::move(a_cancelled)), a_start);
	}
}

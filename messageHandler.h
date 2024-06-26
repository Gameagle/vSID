/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim auf vSID is to ease the work of any controller that edits and assigns
SIDs to flightplans.

Copyright (C) 2024 Gameagle (Philip Maier)
Repo @ https://github.com/Gameagle/vSID

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include <string>
#include <memory>
#include <vector>

namespace vsid
{
	/**
	 * @brief Class that manages message output into ES chat
	 * 
	 */
	class MessageHandler
	{
	public:
		MessageHandler();
		virtual ~MessageHandler();

		enum Level
		{
			Debug,
			Info,
			Warning,
			Error
		};

		enum DebugArea
		{
			All,
			Sid,
			Rwy,
			Atc,
			Req,
			Dev
		};

		/**
		 * @brief Writes message to the local storage - this can be used to forward messages to ES chat
		 * 
		 * @param sender front of msg, usually a level of DEBUG, ERROR, WARNING, INFO
		 * @param msg 
		 */
		void writeMessage(std::string sender, std::string msg, DebugArea debugArea = DebugArea::All);
		/**
		 * @brief Retrieve the first message from the local message stack
		 * 
		 * @return
		 */
		std::pair<std::string, std::string> getMessage();
		/**
		 * @brief Deletes the entire message stack
		 * 
		 */
		void dropMessage();
		/**
		 * @brief Opens a console for debugging messages
		 * 
		 */
		void openConsole();
		/**
		 * @brief Closes a opened console
		 * 
		 */
		void closeConsole();
		/**
		 * @brief Get the current message Level
		 * 
		 * @return int 
		 */
		Level getLevel() const;
		/**
		 * @brief Set the current message Level
		 * 
		 * @param lvl - "DEBUG" / "INFO"
		 */
		void setLevel(std::string lvl);

		inline DebugArea getDebugArea() const { return this->debugArea; }

		bool setDebugArea(std::string debugArea);

	private:
		/**
		 * @brief Local message stack
		 * 
		 */
		std::vector<std::pair<std::string, std::string>> msg;
		FILE* consoleFile = {}; // console file
		Level currentLevel = Level::Debug;
		DebugArea debugArea = DebugArea::All;
	};

	extern std::unique_ptr<vsid::MessageHandler> messageHandler; // definition - needs to be extern to be accessible from all files
}

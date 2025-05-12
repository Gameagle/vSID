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
#include <set>
#include <map>
#include <memory>
#include <vector>

#include <source_location> // #dev - add source_location

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
			Conf,
			Menu,
			Dev
		};

		/**
		 * @brief Writes message to the local storage - this can be used to forward messages to ES chat
		 * 
		 * @param sender front of msg, usually a level of DEBUG, ERROR, WARNING, INFO
		 * @param msg 
		 */
		void writeMessage(std::string sender, std::string msg, DebugArea debugArea = DebugArea::All);

		void writeMessage(std::string sender, std::string msg, const std::source_location& loc, DebugArea debugArea = DebugArea::All);
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

		//************************************
		// Description: Retrieves all cached fpln errors for a callsign (or none if the callsign is missing)
		// Method:    getFplnErrors
		// FullName:  vsid::MessageHandler::getFplnErrors
		// Access:    public 
		// Returns:   std::set<std::string>
		// Qualifier:
		// Parameter: const std::string & callsign
		//************************************
		inline std::set<std::string> getFplnErrors(const std::string& callsign)
		{
			if (this->fplnErrors.contains(callsign)) return this->fplnErrors[callsign];
			else return {};
		}

		//************************************
		// Description: Adds a fpln error to caching
		// Method:    addFplnError
		// FullName:  vsid::MessageHandler::addFplnError
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: const std::string & error
		//************************************
		inline void addFplnError(const std::string& callsign, const std::string& error)
		{
			this->fplnErrors[callsign].insert(error);
		}

		//************************************
		// Description: Deletes a cached fpln error for a callsign if the callsign exists
		// Method:    removeFplnError
		// FullName:  vsid::MessageHandler::removeFplnError
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		// Parameter: const std::string & error
		//************************************
		inline void removeFplnError(const std::string& callsign, const std::string& error)
		{
			if (this->fplnErrors.contains(callsign))
			{
				if (this->fplnErrors[callsign].contains(error)) this->fplnErrors[callsign].erase(error);
				if (this->fplnErrors[callsign].size() == 0) this->fplnErrors.erase(callsign);
			}
		}

		//************************************
		// Description: Clears the callsign from cached errors if it exists
		// Method:    removeCallsignFromErrors
		// FullName:  vsid::MessageHandler::removeCallsignFromErrors
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & callsign
		//************************************
		inline void removeCallsignFromErrors(const std::string& callsign)
		{
			if (this->fplnErrors.contains(callsign)) this->fplnErrors.erase(callsign);
		}

		//************************************
		// Description: Check if a general error is cached
		// Method:    genErrorsContains
		// FullName:  vsid::MessageHandler::genErrorsContains
		// Access:    public 
		// Returns:   bool
		// Qualifier:
		// Parameter: const std::string & error
		//************************************
		inline bool genErrorsContains(const std::string& error)
		{
			return this->genErrors.contains(error);
		}

		//************************************
		// Description: adds a general error to caching
		// Method:    addGenError
		// FullName:  vsid::MessageHandler::addGenError
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & error
		//************************************
		inline void addGenError(const std::string& error)
		{
			this->genErrors.insert(error);
		}

		//************************************
		// Description: removes a stored general error if cached
		// Method:    removeGenError
		// FullName:  vsid::MessageHandler::removeGenError
		// Access:    public 
		// Returns:   void
		// Qualifier:
		// Parameter: const std::string & error
		//************************************
		inline void removeGenError(const std::string& error)
		{
			this->genErrors.erase(error);
		}

	private:
		/**
		 * @brief Local message stack
		 * 
		 */
		std::vector<std::pair<std::string, std::string>> msg;
		FILE* consoleFile = {}; // console file
		Level currentLevel = Level::Debug;
		DebugArea debugArea = DebugArea::All;
		std::map<std::string, std::set<std::string>> fplnErrors;
		std::set<std::string> genErrors;
	};

	extern std::unique_ptr<vsid::MessageHandler> messageHandler; // definition - needs to be extern to be accessible from all files
}

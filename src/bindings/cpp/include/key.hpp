#ifndef CPP_KEY_H
#define CPP_KEY_H

#include <sstream>
#include <string>
#include <cstdarg>

#include <kdbextension.h>

namespace kdb {

class KeyException : public std::exception
{
	const char* what() {return "Key Exception";}
};

class KeyInvalidName : public KeyException
{
	const char* what() {return "Invalid Keyname";}
};

class KeyMetaException : public KeyException
{
	const char* what() {return "Key Meta Data related Exception";}
};

class KeyNoSuchMeta : public KeyMetaException
{
	const char* what() {return "No such meta data";}
};

class KeyBadMeta : public KeyMetaException
{
	const char* what() {return "Could not convert bad meta data";}
};

/**
 * Key represents an elektra key.
 * */
class Key
{
public:
	typedef void (*func_t)();

	Key () :key (ckdb::keyNew (0)) { operator++(); }
	Key (ckdb::Key *k) :key(k) { operator++(); }
	Key (Key &k) :key (k.key) { operator++(); }
	Key (const Key &k) :key (k.key) { operator++(); }
	Key (const char * name_, va_list ap);

	/**
	 * @copydoc keyNew
	 */
	Key (const char * keyName, ...);
	Key (const std::string name_, ...);

	void del () { operator --(); ckdb::keyDel(key); }
	void copy (const Key &other) { ckdb::keyCopy(key,other.key); }
	void clear () { ckdb::keyCopy(key,0); }
	~Key () { del(); }

	void operator ++(int) { operator++(); }
	void operator ++() { ckdb::keyIncRef(key); }

	void operator --(int) { operator--(); }
	void operator --() { ckdb::keyDecRef(key); }

	template <class T>
	T get()
	{
		T x;
		std::string str;
		str = getString();
		std::istringstream ist(str);
		ist >> x;	// convert string to type
		return x;
	}

	template <class T>
	void set(T x)
	{
		std::string str;
		std::ostringstream ost;
		ost << x;	// convert type to string
		setString (ost.str());
	}

	/*Passes out the pointer, maybe directly changing this object*/
	ckdb::Key* getKey () const { return key; }
	ckdb::Key* operator* () const { return key; }
	ckdb::Key* dup () const { return ckdb::keyDup(getKey()); }

	Key& operator= (ckdb::Key *k);
	Key& operator= (const Key &k);

	Key& operator=  (const std::string &name_);
	Key& operator+= (const std::string &name_);
	Key& operator-= (const std::string &name_);

	Key& operator=  (const char *name_);
	Key& operator+= (const char *name_);
	Key& operator-= (const char *name_);

	bool operator ==(const Key &k) const { return ckdb::keyCmp(key, k.key) == 0; }
	bool operator !=(const Key &k) const { return ckdb::keyCmp(key, k.key) != 0; }
	bool operator < (const Key& other) const {return ckdb::keyCmp(key, other.key) < 0; }
	bool operator <= (const Key& other) const {return ckdb::keyCmp(key, other.key) <= 0; }
	bool operator > (const Key& other) const {return ckdb::keyCmp(key, other.key) > 0; }
	bool operator >= (const Key& other) const {return ckdb::keyCmp(key, other.key) >= 0; }
	/**
	 * This is for loops and lookups only.
	 * For loops it checks if there are still more keys.
	 * For lookups it checks if a key could be found.
	 *
	 * @return false on null keys
	 * @return true otherwise
	 */
	operator bool () const {return key != 0;}

	std::string getName() const;
	const char* name() const;
	size_t getNameSize() const;
	/**
	  * @returns true if the key has a valid name
	  */
	bool isValid() const;

	std::string getBaseName() const;
	const char* baseName() const;
	size_t getBaseNameSize() const;

	std::string getDirName() const;

	void setName (const std::string &name_);
	void setBaseName (const std::string &basename);
	void addBaseName (const std::string &basename);

	size_t getFullNameSize() const;
	std::string getFullName() const;

	template <class T>
	T getMeta(const std::string &name_)
	{
		T x;
		std::string str;
		const char *v = 
			static_cast<const char*>(
				ckdb::keyValue(
					ckdb::keyGetMeta(key, name_.c_str())
					)
				);
		if (!v) throw KeyNoSuchMeta();
		str = std::string(v);
		std::istringstream ist(str);
		ist >> x;	// convert string to type
		if (ist.fail()) throw KeyBadMeta();
		return x;
	}

	/*
	const Key *getMeta(const std::string &name_)
	{
		return ckdb::keyGetMeta(key, name_.c_str());
	}
	*/

	template <class T>
	void setMeta(const std::string &name_, T x)
	{
		std::string str;
		std::ostringstream ost;
		ost << x;	// convert type to string
		ckdb::keySetMeta(key, name_.c_str(), ost.str().c_str());
	}

	void copyMeta(const Key &other, const std::string &name_)
	{
		ckdb::keyCopyMeta(key, other.key, name_.c_str());
	}

	void copyAllMeta(const Key &other)
	{
		ckdb::keyCopyAllMeta(key, other.key);
	}

	void rewindMeta () const;
	const Key nextMeta ();
	const Key currentMeta () const;

	std::string getComment() const;
	const char* comment() const;
	size_t getCommentSize() const;
	void setComment(const std::string &comment);

	uid_t getUID() const;
	void setUID(uid_t uid);

	gid_t getGID() const;
	void setGID(gid_t gid);

	mode_t getMode() const;
	void setMode(mode_t mode);

	std::string getOwner() const;
	const char* owner() const;
	size_t getOwnerSize() const;
	void setOwner(const std::string &owner);

	const void* value() const;
	size_t getValueSize() const;

	std::string getString() const;
	void setString(std::string newString);

	func_t getFunc() const;
	std::string getBinary() const;

	size_t getBinary(void *returnedBinary, size_t maxSize) const;
	size_t setBinary(const void *newBinary, size_t dataSize);

	void setDir ();

	void setMTime(time_t time);
	void setATime(time_t time);
	void setCTime(time_t time);

	time_t getMTime() const;
	time_t getATime() const;
	time_t getCTime() const;

	bool isSystem() const;
	bool isUser() const;

	int getNamespace() const;
	size_t getReference() const;

	bool isDir() const;
	bool isString() const;
	bool isBinary() const;

	bool isInactive() const;
	bool isBelow(const Key &k) const;
	bool isDirectBelow(const Key &k) const;

	bool needSync() const;
	bool needRemove() const;
	bool needStat() const;

private:
	ckdb::Key * key; // holds elektra key struct
};

/**@note don't forget the const: getMeta<const ckdb::Key*>*/
template<>
inline const ckdb::Key* Key::getMeta(const std::string &name_)
{
	return
		ckdb::keyGetMeta(key, name_.c_str());
}

/**@note don't forget the const: getMeta<const kdb::Key>*/
template<>
inline const Key Key::getMeta(const std::string &name_)
{
	return
		Key (
			const_cast<ckdb::Key*>(
				ckdb::keyGetMeta(key, name_.c_str())
				)
			);
}

template<>
inline const char* Key::getMeta(const std::string &name_)
{
	return
		static_cast<const char*>(
			ckdb::keyValue(
				ckdb::keyGetMeta(key, name_.c_str())
				)
			);
}

/* We dont want only the first part of the string */
template<>
inline std::string Key::getMeta(const std::string &name_)
{
	std::string str;
	const char *v = 
		static_cast<const char*>(
			ckdb::keyValue(
				ckdb::keyGetMeta(key, name_.c_str())
				)
			);
	if (!v) throw KeyNoSuchMeta();
	str = std::string(v);
	return str;
}

/*Example for an template specialisation.
  Because mode_t is in fact an int, this would
  also change all other int types.
template<>
inline mode_t Key::getMeta(const std::string &name_)
{
	mode_t x;
	std::string str;
	str = std::string(
		static_cast<const char*>(
			ckdb::keyValue(
				ckdb::keyGetMeta(key, name_.c_str())
				)
			)
		);
	std::istringstream ist(str);
	ist >> std::oct >> x;	// convert string to type
	return x;
}
*/

inline void Key::rewindMeta() const
{
	ckdb::keyRewindMeta(key);
}


inline const Key Key::nextMeta()
{
	const ckdb::Key *k = ckdb::keyNextMeta(key);
	return Key(const_cast<ckdb::Key*>(k));
}

inline const Key Key::currentMeta() const
{
	return Key(
		const_cast<ckdb::Key*>(
			ckdb::keyCurrentMeta(const_cast<const ckdb::Key*>(
				key)
				)
			)
		);
}

inline Key::Key (const char * str, va_list ap)
{
	key = ckdb::keyVNew (str, ap);

	operator++();
}

inline Key::Key (const char * str, ...)
{
	va_list ap;

	va_start(ap, str);
	key = ckdb::keyVNew (str, ap);
	va_end(ap);

	operator++();
}

inline Key::Key (const std::string str, ...)
{
	va_list ap;

	va_start(ap, str);
	key = ckdb::keyVNew (str.c_str(), ap);
	va_end(ap);

	operator++();
}

inline Key& Key::operator= (ckdb::Key *k)
{
	if (key != k)
	{
		del();
		key = k;
		operator++();
	}
	return *this;
}

inline Key& Key::operator= (const Key &k)
{
	if (this != &k)
	{
		del();
		key = k.key;
		operator++();
	}
	return *this;
}

inline Key& Key::operator= (const std::string &name_)
{
	ckdb::keySetName(getKey(), name_.c_str());
	return *this;
}

inline Key& Key::operator+= (const std::string &name_)
{
	ckdb::keyAddBaseName(getKey(), name_.c_str());
	return *this;
}

inline Key& Key::operator-= (const std::string &name_)
{
	ckdb::keySetBaseName(getKey(), name_.c_str());
	return *this;
}

inline Key& Key::operator= (const char *name_)
{
	ckdb::keySetName(getKey(), name_);
	return *this;
}

inline Key& Key::operator+= (const char *name_)
{
	ckdb::keyAddBaseName(getKey(), name_);
	return *this;
}

inline Key& Key::operator-= (const char *name_)
{
	ckdb::keySetBaseName(getKey(), name_);
	return *this;
}

inline size_t Key::getNameSize() const
{
	return ckdb::keyGetNameSize (getKey());
}

inline std::string Key::getName() const
{
	return std::string (ckdb::keyName(key));
}

inline const char* Key::name() const
{
	return ckdb::keyName(getKey());
}

inline bool Key::isValid() const
{
	return ckdb::keyGetNameSize (getKey()) > 1;
}

inline size_t Key::getBaseNameSize() const
{
	return ckdb::keyGetBaseNameSize (getKey());
}

inline std::string Key::getBaseName() const
{
	return std::string (ckdb::keyBaseName(key));
}

inline std::string Key::getDirName() const
{
	std::string ret = ckdb::keyName(key);
	return ret.substr(0, ret.rfind('/'));
}


inline const char* Key::baseName() const
{
	return ckdb::keyBaseName(getKey());
}


/**Sets a name_ for a key.
 * Throws kdb::KeyInvalidName when the name_ is not valid*/
inline void Key::setName (const std::string &name_)
{
	if (ckdb::keySetName (getKey(), name_.c_str()) == -1)
		throw KeyInvalidName();
}

/**Sets a base name_ for a key.
 * Throws kdb::KeyInvalidName when the name_ is not valid*/
inline void Key::setBaseName (const std::string &name_)
{
	if (ckdb::keySetBaseName (getKey(), name_.c_str()) == -1)
		throw KeyInvalidName();
}

inline void Key::addBaseName (const std::string &name_)
{
	if (ckdb::keyAddBaseName (getKey(), name_.c_str()) == -1)
		throw KeyInvalidName();
}

inline size_t Key::getFullNameSize() const
{
	return ckdb::keyGetFullNameSize (getKey());
}

inline std::string Key::getFullName() const
{
	ssize_t csize = ckdb::keyGetFullNameSize (getKey());
	if (csize == -1) return "";
	std::string str;
	char * field = new char [csize];

	ckdb::keyGetFullName (getKey(), field, csize);
	str = field;
	delete [] field;
	return str;
}

/**Returns the comment for the key.*/
inline std::string Key::getComment() const
{
	return std::string(ckdb::keyComment(key));
}

inline const char* Key::comment() const
{
	return ckdb::keyComment (key);
}

/**Returns the size of the comment*/
inline size_t Key::getCommentSize() const
{
	return ckdb::keyGetCommentSize (key);
}

/**Sets a comment for the specified key.*/
inline void Key::setComment(const std::string &comment_)
{
	ckdb::keySetComment (getKey(), comment_.c_str());
}

/**Returns the UID of the the key. It always
 * returs the current UID*/
inline uid_t Key::getUID() const
{
	return ckdb::keyGetUID (getKey());
}

/**Sets another UID for a key. This will always
 * fail, because you are a user.*/
inline void Key::setUID(uid_t uid)
{
	ckdb::keySetUID (getKey(), uid);
}

/**Gets the Groupid from a specific key.*/
inline gid_t Key::getGID() const
{
	return ckdb::keyGetGID (getKey());
}

/**Sets the Groupid for a specific key. Only
 * groups where the user is a member a valid.*/
inline void Key::setGID(gid_t gid)
{
	ckdb::keySetGID (getKey(), gid);
}

/**Returns the Mode of a key.
 * It is based on 4 Octets: special, user, group and other
 * The first bit is execute (not used for kdb)
 * The second bit is write mode.
 * The third bit is read mode.
 * See more in chmod (2).
 * An easier description may be in chmod (1), but
 * the +-rwx method is not supported.*/
inline mode_t Key::getMode() const
{
	return ckdb::keyGetMode(getKey());
}

/**Sets the Mode of a key. For more info see
 * getMode (std::string ).*/
inline void Key::setMode(mode_t mode)
{
	ckdb::keySetMode (getKey(),mode);
}

/**Returns the Owner of the Key. It will always return
 * the current user.*/
inline std::string Key::getOwner() const
{
	return std::string (ckdb::keyOwner(key));
}

inline const char* Key::owner() const
{
	return ckdb::keyOwner(key);
}

inline size_t Key::getOwnerSize() const
{
	return ckdb::keyGetOwnerSize(key);
}

/**Sets the Owner of the Key. It will fail, because
 * you are not root.*/
inline void Key::setOwner(const std::string &owner_)
{
	ckdb::keySetOwner(getKey(), owner_.c_str());
}

inline const void*Key::value() const
{
	return ckdb::keyValue (key);
}

/**Returns the DataSize of the Binary or String. It is used
 * for the internal malloc().*/
inline size_t Key::getValueSize() const
{
	return ckdb::keyGetValueSize (key);
}

/** Returns the string directly from the key.
 * It should be the same as get().
 * @return empty string on null pointers
 */
inline std::string Key::getString() const
{
	ssize_t csize =  ckdb::keyGetValueSize (getKey());
	if (csize == -1) return "";
	char * field = new char [csize];

	if (ckdb::keyGetString (getKey(), field, csize) == -1)
	{
		delete [] field;
		return "(binary)";
	}
	std::string str (field);
	delete [] field;
	return str;
}

inline Key::func_t Key::getFunc() const
{
	union {Key::func_t f; void* v;} conversation;

	if (ckdb::keyGetBinary(getKey(),
			&conversation.v,
			sizeof(conversation)) != sizeof(conversation))
				return 0;

	return conversation.f;
}

/**Sets the String of a key.*/
inline void Key::setString(std::string newString)
{
	ckdb::keySetString (getKey(), newString.c_str());
}

/**Returns the binary Value of the key. It will not be encoded
 * or decoded.*/
inline size_t Key::getBinary(void *returnedBinary, size_t maxSize) const
{
	return ckdb::keyGetBinary (getKey(), returnedBinary, maxSize);
}

/**Returns the binary Value of the key.
 * It will not be encoded or decoded.
 * @retval "" on null pointer*/
inline std::string Key::getBinary() const
{
	ssize_t csize = getValueSize();
	if (csize == -1) return "";
	char *buffer = new char[csize];
	ckdb::keyGetBinary (getKey(), buffer, csize);
	std::string str (buffer, csize);
	delete []buffer;
	return str;
}

/**Sets a binary Value of a key*/
inline size_t Key::setBinary(const void *newBinary, size_t dataSize)
{	
	size_t s = ckdb::keySetBinary (getKey(), newBinary, dataSize);
	return s;
}

inline void Key::setDir ()
{
	ckdb::keySetDir (key);
}

inline void Key::setMTime (time_t time)
{
	ckdb::keySetMTime (key, time);
}

inline void Key::setATime (time_t time)
{
	ckdb::keySetATime (key, time);
}

inline void Key::setCTime (time_t time)
{
	ckdb::keySetCTime (key, time);
}

/**Returns the time the Key was modified*/
inline time_t Key::getMTime() const
{
	return ckdb::keyGetMTime(key);
}

/**Returns the last access time.*/
inline time_t Key::getATime() const
{
	return ckdb::keyGetATime(key);
}

/**Returns when the Key last was changed.*/
inline time_t Key::getCTime() const
{
	return ckdb::keyGetCTime(key);
}

inline bool Key::isSystem() const
{
	return ckdb::keyIsSystem(key);
}

inline bool Key::isUser() const
{
	return ckdb::keyIsUser(key);
}

inline size_t Key::getReference() const
{
	return ckdb::keyGetRef(key);
}

inline bool Key::isDir() const
{
	return ckdb::keyIsDir(key);
}

inline bool Key::isString() const
{
	return ckdb::keyIsString(key);
}

inline bool Key::isBinary() const
{
	return ckdb::keyIsBinary(key);
}

inline bool Key::isInactive () const
{
	return ckdb::keyIsInactive (key);
}

inline bool Key::isBelow(const Key & k) const
{
	return ckdb::keyIsBelow(key, k.getKey());
}

inline bool Key::isDirectBelow(const Key & k) const
{
	return ckdb::keyIsDirectBelow(key, k.getKey());
}

inline bool Key::needSync() const
{
	return ckdb::keyNeedSync(key);
}

} // end of namespace kdb

#endif


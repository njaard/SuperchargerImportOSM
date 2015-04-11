#ifndef XMLTREE_H
#define XMLTREE_H

#include <Qt/qxmlstream.h>
#include <Qt/qtextstream.h>

#include <set>
#include <stdexcept>

class XmlTree
{
	QXmlStreamReader* reader;
public:
	struct Node
	{
		friend class XmlTree;
	private:
		mutable XmlTree *tree;
		Node(XmlTree *tree) : tree(tree) { }
	public:
		Node() :tree(0) { }
		Node(const Node &n) : tree(n.tree) { }
		~Node() { if (tree) tree->next(); }
		Node & operator*(const Node &left)
		{
			left.tree=0;
			return *this;
		}
	};
	
private:
	struct Sequence
	{
		virtual ~Sequence() { }
		virtual void activate(QXmlStreamReader *e)=0;
		
		QXmlStreamReader::TokenType type() const { return mType; }
	protected:
		QXmlStreamReader::TokenType mType;
	};

	template<typename Fn>
	class SElement : public Sequence
	{
		const QString name;
		const Fn f;
	public:
		SElement(const QString &name, Fn &fn) : name(name), f(fn) { mType = QXmlStreamReader::StartElement; }
		virtual void activate(QXmlStreamReader *e)
		{
			if (e->name() == name)
				f(e);
		}
	};
	template<typename Fn>
	class SText : public Sequence
	{
		const Fn f;
	public:
		SText(Fn &fn) : f(fn) { mType = QXmlStreamReader::Characters; }
		virtual void activate(QXmlStreamReader *e)
		{
			f(e);
		}
	};
	
	std::vector<std::set<Sequence*>*> sequenceStack;

	static void clearSeq(std::set<Sequence*> &seq)
	{
		for (std::set<Sequence*>::iterator i =seq.begin(); i != seq.end(); ++i)
			delete *i;
	}
	
	std::vector<QXmlStreamReader::TokenType> domStack;
	
	void next()
	{
		const unsigned stackSize = domStack.size();
		
		bool repeat=true;
		while (repeat)
		{
			QXmlStreamReader::TokenType t = reader->readNext();
			
			if (t==QXmlStreamReader::Invalid)
				throw std::runtime_error(reader->errorString().toUtf8().constData());
			if (t==QXmlStreamReader::EndDocument)
				repeat=false;
			else if ( t == QXmlStreamReader::StartElement)
			{
				domStack.push_back(t);
				std::set<Sequence*> *seq = sequenceStack.back();
				sequenceStack.push_back( new std::set<Sequence*>() );

				for (std::set<Sequence*>::iterator i =seq->begin(); i != seq->end(); ++i)
				{
						Sequence *const s = *i;
					if (s->type() == QXmlStreamReader::StartElement)
						s->activate(reader);
				}
			}
			else if ( t == QXmlStreamReader::EndElement)
			{
				if (stackSize == domStack.size())
					repeat=false;
				clearSeq(*sequenceStack.back());
				delete sequenceStack.back();
				sequenceStack.pop_back();
				domStack.pop_back();
			}
			else if ( t == QXmlStreamReader::Characters)
			{
				domStack.push_back(t);
				std::set<Sequence*> *seq = sequenceStack.back();
				std::set<Sequence*> nSeq;
				sequenceStack.push_back(&nSeq);

				for (std::set<Sequence*>::iterator i =seq->begin(); i != seq->end(); ++i)
				{
					Sequence *const s = *i;
					if (s->type() == QXmlStreamReader::Characters)
						s->activate(reader);
				}
				clearSeq(*sequenceStack.back());
				sequenceStack.pop_back();
				domStack.pop_back();
			}
		}
	}

public:
	XmlTree(QXmlStreamReader* reader)
		: reader(reader)
	{
		sequenceStack.push_back( new std::set<Sequence*>() );
	}
	
	~XmlTree()
	{
		clearSeq(*sequenceStack.back());
		delete sequenceStack.back();
	}
	
	template <typename Fn>
	Node element(const QString &name, Fn fn)
	{
		sequenceStack.back()->insert(new SElement<Fn>(name, fn));
		
		return Node(this);
	}

	template <typename Fn>
	Node text(Fn fn)
	{
		sequenceStack.back()->insert(new SText<Fn>(fn));
		
		return Node(this);
	}

};

#endif

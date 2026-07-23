// Project Seoul Library, Boards, and Live Collections.

#include "seoul/browser/library/library_types.h"

namespace seoul {

// Out-of-line copy, move, and destruction for the Library value structs. Every
// member is deep-copyable, so all six special members are member-wise.
BoardElement::BoardElement() = default;
BoardElement::BoardElement(const BoardElement&) = default;
BoardElement::BoardElement(BoardElement&&) = default;
BoardElement& BoardElement::operator=(const BoardElement&) = default;
BoardElement& BoardElement::operator=(BoardElement&&) = default;
BoardElement::~BoardElement() = default;

BoardRecord::BoardRecord() = default;
BoardRecord::BoardRecord(const BoardRecord&) = default;
BoardRecord::BoardRecord(BoardRecord&&) = default;
BoardRecord& BoardRecord::operator=(const BoardRecord&) = default;
BoardRecord& BoardRecord::operator=(BoardRecord&&) = default;
BoardRecord::~BoardRecord() = default;

LibraryArtifact::LibraryArtifact() = default;
LibraryArtifact::LibraryArtifact(const LibraryArtifact&) = default;
LibraryArtifact::LibraryArtifact(LibraryArtifact&&) = default;
LibraryArtifact& LibraryArtifact::operator=(const LibraryArtifact&) = default;
LibraryArtifact& LibraryArtifact::operator=(LibraryArtifact&&) = default;
LibraryArtifact::~LibraryArtifact() = default;

LiveCollectionDefinition::LiveCollectionDefinition() = default;
LiveCollectionDefinition::LiveCollectionDefinition(
    const LiveCollectionDefinition&) = default;
LiveCollectionDefinition::LiveCollectionDefinition(LiveCollectionDefinition&&) =
    default;
LiveCollectionDefinition& LiveCollectionDefinition::operator=(
    const LiveCollectionDefinition&) = default;
LiveCollectionDefinition& LiveCollectionDefinition::operator=(
    LiveCollectionDefinition&&) = default;
LiveCollectionDefinition::~LiveCollectionDefinition() = default;

LiveCollectionItem::LiveCollectionItem() = default;
LiveCollectionItem::LiveCollectionItem(const LiveCollectionItem&) = default;
LiveCollectionItem::LiveCollectionItem(LiveCollectionItem&&) = default;
LiveCollectionItem& LiveCollectionItem::operator=(const LiveCollectionItem&) =
    default;
LiveCollectionItem& LiveCollectionItem::operator=(LiveCollectionItem&&) =
    default;
LiveCollectionItem::~LiveCollectionItem() = default;

LiveCollectionRecord::LiveCollectionRecord() = default;
LiveCollectionRecord::LiveCollectionRecord(const LiveCollectionRecord&) =
    default;
LiveCollectionRecord::LiveCollectionRecord(LiveCollectionRecord&&) = default;
LiveCollectionRecord& LiveCollectionRecord::operator=(
    const LiveCollectionRecord&) = default;
LiveCollectionRecord& LiveCollectionRecord::operator=(LiveCollectionRecord&&) =
    default;
LiveCollectionRecord::~LiveCollectionRecord() = default;

}  // namespace seoul

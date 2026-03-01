#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <iomanip>

using namespace std;

// ---------- Helpers ----------
static string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(),
              [](unsigned char c) { return (char)tolower(c); });
    return s;
}

static bool containsIC(const string& text, const string& q) {
    return toLower(text).find(toLower(q)) != string::npos;
}

static void printDate(const chrono::system_clock::time_point& tp) {
    time_t t = chrono::system_clock::to_time_t(tp);
    tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    cout << put_time(&local_tm, "%Y-%m-%d");
}

// ---------- Book Status ----------
enum class BookStatus { Available, Borrowed, Reserved };

static string statusToString(BookStatus s) {
    switch (s) {
        case BookStatus::Available: return "Available";
        case BookStatus::Borrowed:  return "Borrowed";
        case BookStatus::Reserved:  return "Reserved";
    }
    return "Unknown";
}

// ---------- OOP: User (Base) ----------
class User {
private:
    int id;
    string name;

public:
    User(int id, string name) : id(id), name(std::move(name)) {}
    virtual ~User() = default;

    int getId() const { return id; }
    const string& getName() const { return name; }

    // Polymorphism
    virtual string role() const = 0;
};

// ---------- Member ----------
class Member : public User {
private:
    vector<int> borrowedBookIds;  // Encapsulated
    vector<int> reservedBookIds;  // Track books reserved by this member

public:
    Member(int id, string name) : User(id, std::move(name)) {}

    string role() const override { return "Member"; }

    int borrowedCount() const { return (int)borrowedBookIds.size(); }
    bool canBorrow() const { return borrowedCount() < 5; }

    void addBorrowed(int bookId) { borrowedBookIds.push_back(bookId); }

    bool removeBorrowed(int bookId) {
        auto it = find(borrowedBookIds.begin(), borrowedBookIds.end(), bookId);
        if (it == borrowedBookIds.end()) return false;
        borrowedBookIds.erase(it);
        return true;
    }

    bool hasBorrowed(int bookId) const {
        return find(borrowedBookIds.begin(), borrowedBookIds.end(), bookId) != borrowedBookIds.end();
    }

    // Reservation tracking
    void addReserved(int bookId) { reservedBookIds.push_back(bookId); }

    bool removeReserved(int bookId) {
        auto it = find(reservedBookIds.begin(), reservedBookIds.end(), bookId);
        if (it == reservedBookIds.end()) return false;
        reservedBookIds.erase(it);
        return true;
    }

    bool hasReserved(int bookId) const {
        return find(reservedBookIds.begin(), reservedBookIds.end(), bookId) != reservedBookIds.end();
    }

    void showBorrowed() const {
        cout << "Borrowed (" << borrowedCount() << "/5): ";
        if (borrowedBookIds.empty()) {
            cout << "None\n";
        } else {
            for (int id : borrowedBookIds) cout << id << " ";
            cout << "\n";
        }

        cout << "Reserved (" << reservedBookIds.size() << "): ";
        if (reservedBookIds.empty()) {
            cout << "None\n";
        } else {
            for (int id : reservedBookIds) cout << id << " ";
            cout << "\n";
        }
    }
};

// ---------- Librarian ----------
class Librarian : public User {
public:
    Librarian(int id, string name) : User(id, std::move(name)) {}
    string role() const override { return "Librarian"; }
};

// ---------- Book ----------
class Book {
private:
    int id;
    string title;
    string author;

    BookStatus status = BookStatus::Available;

    // Borrow info
    bool hasBorrower = false;
    int borrowerMemberId = -1;
    chrono::system_clock::time_point dueDate{};

    // Reservation info
    bool hasReservation = false;
    int reservedByMemberId = -1;
    chrono::system_clock::time_point reservationExpiry{};

public:
    Book(int id, string title, string author)
        : id(id), title(std::move(title)), author(std::move(author)) {}

    int getId() const { return id; }
    const string& getTitle() const { return title; }
    const string& getAuthor() const { return author; }
    BookStatus getStatus() const { return status; }

    bool isAvailable() const { return status == BookStatus::Available; }
    bool isBorrowed() const { return status == BookStatus::Borrowed; }
    bool isReserved() const { return status == BookStatus::Reserved; }

    bool canRemove() const {
        return status != BookStatus::Borrowed; // don't remove borrowed books
    }

    bool reservationValid(const chrono::system_clock::time_point& now) const {
        return hasReservation && now <= reservationExpiry;
    }

    bool isOverdue(const chrono::system_clock::time_point& now) const {
        return status == BookStatus::Borrowed && hasBorrower && now > dueDate;
    }

    bool borrowedBy(int memberId) const {
        return hasBorrower && borrowerMemberId == memberId;
    }

    // Borrow: only if Available
    bool borrow(int memberId, const chrono::system_clock::time_point& now) {
        if (status != BookStatus::Available) return false;

        status = BookStatus::Borrowed;
        hasBorrower = true;
        borrowerMemberId = memberId;
        dueDate = now + chrono::hours(24 * 14); // 14 days
        return true;
    }

    // Reserve: only if Borrowed and not already reserved
    bool reserve(int memberId, const chrono::system_clock::time_point& now) {
        if (status != BookStatus::Borrowed) return false;
        if (hasReservation) return false;

        hasReservation = true;
        reservedByMemberId = memberId;
        reservationExpiry = now + chrono::hours(24 * 3); // 3 days
        return true;
    }

    // Return book; may become Reserved if valid reservation exists.
    // notifyMemberId will be set to reserving member id if reservation becomes active.
    bool returnBack(const chrono::system_clock::time_point& now, int& notifyMemberId) {
        if (status != BookStatus::Borrowed) return false;

        // clear borrow
        status = BookStatus::Available;
        hasBorrower = false;
        borrowerMemberId = -1;

        // if reservation valid -> make Reserved and notify
        if (reservationValid(now)) {
            status = BookStatus::Reserved;
            notifyMemberId = reservedByMemberId;
        } else {
            // clear expired reservation (if any) — including stale expiry timestamp
            hasReservation = false;
            reservedByMemberId = -1;
            reservationExpiry = {};  // FIX: reset to epoch to avoid stale timestamp
        }
        return true;
    }

    // When a reserved member "collects", they should borrow it (optional feature)
    // For simplicity we just allow borrowing from Reserved by the reserving member.
    bool collectReservedAndBorrow(int memberId, const chrono::system_clock::time_point& now) {
        if (status != BookStatus::Reserved) return false;
        if (!hasReservation) return false;
        if (!reservationValid(now)) {
            // expired
            status = BookStatus::Available;
            hasReservation = false;
            reservedByMemberId = -1;
            return false;
        }
        if (reservedByMemberId != memberId) return false;

        // clear reservation then borrow
        hasReservation = false;
        reservedByMemberId = -1;
        status = BookStatus::Available;
        return borrow(memberId, now);
    }

    void print(const chrono::system_clock::time_point& now) const {
        cout << "[" << id << "] " << title << " by " << author
             << " | " << statusToString(status);

        if (status == BookStatus::Borrowed && hasBorrower) {
            cout << " | Borrower: " << borrowerMemberId << " | Due: ";
            printDate(dueDate);
            if (isOverdue(now)) cout << " (OVERDUE)";
        }

        if (hasReservation) {
            cout << " | ReservedBy: " << reservedByMemberId << " | Exp: ";
            printDate(reservationExpiry);
            if (!reservationValid(now)) cout << " (EXPIRED)";
        }

        cout << "\n";
    }
};

// ---------- Library ----------
class Library {
private:
    vector<Book> books;
    int nextBookId = 1;

    Book* findBook(int bookId) {
        for (auto& b : books) if (b.getId() == bookId) return &b;
        return nullptr;
    }

public:
    void seed() {
        addBook("Harry Potter and the Philosopher's Stone", "J.K. Rowling");
        addBook("The Alchemist", "Paulo Coelho");
        addBook("To Kill a Mockingbird", "Harper Lee");
        addBook("The Great Gatsby", "F. Scott Fitzgerald");
        addBook("Atomic Habits", "James Clear");
    }

    void addBook(const string& title, const string& author) {
        books.emplace_back(nextBookId++, title, author);
    }

    bool removeBook(int bookId) {
        auto it = find_if(books.begin(), books.end(),
                          [&](const Book& b) { return b.getId() == bookId; });
        if (it == books.end()) return false;
        if (!it->canRemove()) return false;
        books.erase(it);
        return true;
    }

    void listAll() const {
        auto now = chrono::system_clock::now();
        cout << "\n--- Books ---\n";
        if (books.empty()) { cout << "No books.\n"; return; }
        for (const auto& b : books) b.print(now);
    }

    void searchAndPrint(const string& q) const {
        auto now = chrono::system_clock::now();
        cout << "\n--- Search Results ---\n";
        int count = 0;
        for (const auto& b : books) {
            if (containsIC(b.getTitle(), q) || containsIC(b.getAuthor(), q)) {
                b.print(now);
                count++;
            }
        }
        if (count == 0) cout << "No matches.\n";
    }

    bool borrowBook(int bookId, Member& member) {
        // If reserved, delegate to collectBook() which also clears reservation tracking
        if (findBook(bookId) && findBook(bookId)->getStatus() == BookStatus::Reserved) {
            return collectBook(bookId, member);
        }

        auto now = chrono::system_clock::now();
        Book* book = findBook(bookId);
        if (!book) { cout << "Book not found.\n"; return false; }

        if (!member.canBorrow()) {
            cout << "Borrow limit reached (max 5).\n";
            return false;
        }
        if (!book->isAvailable()) {
            cout << "Book is not available.\n";
            return false;
        }

        if (!book->borrow(member.getId(), now)) {
            cout << "Borrow failed.\n";
            return false;
        }

        member.addBorrowed(bookId);
        cout << "Borrowed successfully.\n";
        return true;
    }

    bool returnBook(int bookId, Member& member) {
        auto now = chrono::system_clock::now();
        Book* book = findBook(bookId);
        if (!book) { cout << "Book not found.\n"; return false; }

        if (!book->borrowedBy(member.getId())) {
            cout << "You cannot return this book (not borrowed by you).\n";
            return false;
        }

        int notifyId = -1;
        if (!book->returnBack(now, notifyId)) {
            cout << "Return failed.\n";
            return false;
        }

        member.removeBorrowed(bookId);
        cout << "Returned successfully.\n";

        if (notifyId != -1) {
            cout << "[Notification] Book " << bookId
                 << " is RESERVED for Member ID " << notifyId
                 << " (valid for 3 days).\n";
        }
        return true;
    }

    // Overload: also clears reservation tracking when reserved member collects
    bool collectBook(int bookId, Member& member) {
        auto now = chrono::system_clock::now();
        Book* book = findBook(bookId);
        if (!book) { cout << "Book not found.\n"; return false; }
        if (!member.canBorrow()) { cout << "Borrow limit reached (max 5).\n"; return false; }

        bool ok = book->collectReservedAndBorrow(member.getId(), now);
        if (!ok) { cout << "This reserved book is not available for you or reservation expired.\n"; return false; }

        member.removeReserved(bookId);  // FIX: clear reservation record from member
        member.addBorrowed(bookId);
        cout << "Collected reserved book and borrowed successfully.\n";
        return true;
    }

    bool reserveBook(int bookId, Member& member) {
        auto now = chrono::system_clock::now();
        Book* book = findBook(bookId);
        if (!book) { cout << "Book not found.\n"; return false; }

        if (book->getStatus() == BookStatus::Available) {
            cout << "Book is Available. You can borrow it instead of reserving.\n";
            return false;
        }
        if (book->getStatus() == BookStatus::Reserved) {
            cout << "Book is already reserved.\n";
            return false;
        }

        if (!book->reserve(member.getId(), now)) {
            cout << "Reserve failed (maybe already reserved).\n";
            return false;
        }

        member.addReserved(bookId);  // FIX: track reservation on the member
        cout << "Reserved successfully (expires in 3 days).\n";
        return true;
    }

    void overdueReport() const {
        auto now = chrono::system_clock::now();
        cout << "\n--- Overdue Report ---\n";
        bool any = false;
        for (const auto& b : books) {
            if (b.isOverdue(now)) {
                b.print(now);
                any = true;
            }
        }
        if (!any) cout << "No overdue books.\n";
    }
};

// ---------- Input ----------
static int readInt(const string& prompt) {
    while (true) {
        cout << prompt;
        string s;
        getline(cin, s);
        try {
            size_t idx = 0;
            int v = stoi(s, &idx);
            if (idx != s.size()) throw runtime_error("bad");
            return v;
        } catch (...) {
            cout << "Invalid input. Enter a number.\n";
        }
    }
}

static void memberMenu(Library& lib, Member& m) {
    while (true) {
        cout << "\n== Member Menu (" << m.getName() << ") ==\n";
        cout << "1) List all books\n";
        cout << "2) Search books\n";
        cout << "3) Borrow book\n";
        cout << "4) Return book\n";
        cout << "5) Reserve book (only if borrowed)\n";
        cout << "6) View my borrowed list\n";
        cout << "0) Logout\n";

        int choice = readInt("Choose: ");
        if (choice == 0) break;

        if (choice == 1) {
            lib.listAll();
        } else if (choice == 2) {
            cout << "Search query: ";
            string q; getline(cin, q);
            lib.searchAndPrint(q);
        } else if (choice == 3) {
            int id = readInt("Book ID to borrow: ");
            lib.borrowBook(id, m);
        } else if (choice == 4) {
            int id = readInt("Book ID to return: ");
            lib.returnBook(id, m);
        } else if (choice == 5) {
            int id = readInt("Book ID to reserve: ");
            lib.reserveBook(id, m);
        } else if (choice == 6) {
            m.showBorrowed();
        } else {
            cout << "Invalid option.\n";
        }
    }
}

static void librarianMenu(Library& lib, Librarian& l) {
    while (true) {
        cout << "\n== Librarian Menu (" << l.getName() << ") ==\n";
        cout << "1) List all books\n";
        cout << "2) Add book\n";
        cout << "3) Remove book\n";
        cout << "4) Overdue report\n";
        cout << "0) Logout\n";

        int choice = readInt("Choose: ");
        if (choice == 0) break;

        if (choice == 1) {
            lib.listAll();
        } else if (choice == 2) {
            cout << "Title: ";
            string title; getline(cin, title);
            cout << "Author: ";
            string author; getline(cin, author);
            lib.addBook(title, author);
            cout << "Book added.\n";
        } else if (choice == 3) {
            int id = readInt("Book ID to remove: ");
            if (lib.removeBook(id)) cout << "Book removed.\n";
            else cout << "Remove failed (not found or currently borrowed).\n";
        } else if (choice == 4) {
            lib.overdueReport();
        } else {
            cout << "Invalid option.\n";
        }
    }
}

// ---------- Main ----------
int main() {
    Library lib;
    lib.seed();

    Member m1(101, "Rohit");
    Member m2(102, "Aisha");
    Librarian l1(201, "Jane");

    while (true) {
        cout << "\n=== Smart Library Management System (SLMS) ===\n";
        cout << "1) Login as Member (Rohit)\n";
        cout << "2) Login as Member (Aisha)\n";
        cout << "3) Login as Librarian (Jane)\n";
        cout << "0) Exit\n";

        int choice = readInt("Choose: ");
        if (choice == 0) break;

        User* current = nullptr;
        if (choice == 1) current = &m1;
        else if (choice == 2) current = &m2;
        else if (choice == 3) current = &l1;
        else { cout << "Invalid.\n"; continue; }

        cout << "Logged in as " << current->getName()
             << " (" << current->role() << ")\n";

        if (current->role() == "Member") {
            // FIX: safe cast — use dynamic_cast instead of fragile ternary
            Member* mm = dynamic_cast<Member*>(current);
            if (mm) memberMenu(lib, *mm);
        } else {
            librarianMenu(lib, l1);
        }
    }

    cout << "Goodbye.\n";
    return 0;
}
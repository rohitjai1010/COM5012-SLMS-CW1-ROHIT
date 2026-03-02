# Requirements (SLMS Partial System)

## Functional Requirements
1. **User Authentication**: Simple login system for Members and Librarians.
2. **Book Management**: Librarians can add, remove, and list books.
3. **Borrowing System**: Members can borrow up to 5 available books.
4. **Return System**: Members can return books, which then become available or transition to reserved status.
5. **Reservation System**: Members can reserve books that are currently borrowed.
6. **Search**: Users can search for books by title or author (case-insensitive).
7. **Overdue Tracking**: System identifies and reports books that have passed their 14-day due date.

## Non-Functional Requirements
1. **Reliability**: Reservations must expire automatically if not collected within 3 days.
2. **Usability**: Simple interactive console menu for easy navigation.
3. **Safety**: Borrowed or Reserved books cannot be removed from the catalog.

## Assumptions / Constraints
1. **In-Memory Storage**: Data is lost once the session ends (no persistent DB).
2. **Simplified Security**: No real password authentication is implemented.
3. **Local Time**: System uses the local system clock for all date calculations.
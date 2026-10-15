
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

int main(void) {
    int num_of_voters = 0;
    int num_of_rounds = 0;

    // Get valid input
    while (num_of_voters < 3) {
        printf("Enter the number of voters: ");
        fflush(stdout);

        if (scanf("%d", &num_of_voters) != 1) {
            fprintf(stderr, "Invalid voter count.\n");
            return 1;
        }
    }

    printf("Enter the number of rounds: ");
    if (scanf("%d", &num_of_rounds) != 1 || num_of_rounds < 1) {
        fprintf(stderr, "Invalid number of rounds.\n");
        return 1;
    }

    // vote_pipe[i]: child i writes, judge reads
    // control_pipe[i]: judge writes, child i reads
    int vote_pipe[num_of_voters][2];
    int control_pipe[num_of_voters][2];
    pid_t pids[num_of_voters];

    // Phase 1: Create all pipes
    for (int i = 0; i < num_of_voters; i++) {
        if (pipe(vote_pipe[i]) == -1 ||
            pipe(control_pipe[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }

    // Phase 2: Create one child per voter
    for (int j = 0; j < num_of_voters; j++) {
        pids[j] = fork();

        if (pids[j] == -1) {
            perror("fork");
            return 1;
        }

        if (pids[j] == 0) {
            // Child: keep only its own vote-write
            // end and control-read end.
            for (int k = 0; k < num_of_voters; k++) {
                close(vote_pipe[k][0]);
                close(control_pipe[k][1]);

                if (k != j) {
                    close(vote_pipe[k][1]);
                    close(control_pipe[k][0]);
                }
            }

            // Give each child a different random sequence.
            srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

            for (int round = 0; round < num_of_rounds; round++) {
                int vote = (rand() % 2);  // 1 = YES, 0 = NO

                printf("Round %d: Voter %d votes: %s\n",round, j, vote ? "YES" : "NO");
                fflush(stdout);

                // Send this round's vote to the judge.
                if (write(vote_pipe[j][1], &vote, sizeof(vote)) != sizeof(vote)) {
                    perror("write vote");
                    _exit(1);
                }

                // Wait for the judge before starting
                // the next round.
                if (round < num_of_rounds - 1) {
                    int proceed;

                    if (read(control_pipe[j][0], &proceed, sizeof(proceed)) != sizeof(proceed)) {
                        perror("read control");
                        _exit(1);
                    }
                }
            }

            close(vote_pipe[j][1]);
            close(control_pipe[j][0]);
            _exit(0);
        }
    }

    // Phase 3: Judge closes its unused pipe ends.
    for (int j = 0; j < num_of_voters; j++) {
        close(vote_pipe[j][1]);
        close(control_pipe[j][0]);
    }

    // Phase 4: Conduct each round.
    for (int round = 0; round < num_of_rounds; round++) {
        int y_votes = 0;

        // Collect one vote from every voter.
        for (int j = 0; j < num_of_voters; j++) {
            int vote;

            if (read(vote_pipe[j][0], &vote, sizeof(vote)) != sizeof(vote)) {
                fprintf(stderr, "Failed to read voter %d's vote.\n", j);
                return 1;
            }

            if (vote == 1) {
                y_votes++;
            }
        }

        // Calculate and print the majority.
        const char *verdict = (y_votes * 2 > num_of_voters) ? "YES" : "NO";
        printf("Round %d: The voting result is a %s.\n\n",round, verdict);
        fflush(stdout);

        // Tell every voter to proceed to the next round.
        if (round < num_of_rounds - 1) {
            int proceed = 1;

            for (int j = 0; j < num_of_voters; j++) {
                if (write(control_pipe[j][1], &proceed, sizeof(proceed)) != sizeof(proceed)) {
                    perror("write control");
                    return 1;
                }
            }
        }
    }

    // Phase 5: Close remaining pipes.
    for (int j = 0; j < num_of_voters; j++) {
        close(vote_pipe[j][0]);
        close(control_pipe[j][1]);
    }

    // Wait for every child to finish.
    for (int j = 0; j < num_of_voters; j++) {
        if (waitpid(pids[j], NULL, 0) == -1) {
            perror("waitpid");
            return 1;
        }
    }

    return 0;
}
